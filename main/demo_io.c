#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include "demo.h"
#include "board_pins.h"
#include "bsp/esp-bsp.h"
#include "driver/uart.h"
#include "esp_check.h"

esp_err_t demo_i2c_scan(char *result, size_t capacity)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), "i2c", "init");
    size_t used = snprintf(result, capacity, "I2C SDA7/SCL8 addresses:");
    for (int addr = 8; addr < 0x78; addr++) {
        if (i2c_master_probe(bsp_i2c_get_handle(), addr, 30) == ESP_OK && used < capacity) {
            used += snprintf(result + used, capacity - used, " 0x%02X", addr);
        }
    }
    return ESP_OK;
}

static i2c_master_dev_handle_t rtc;
static esp_err_t rtc_init(void)
{
    if (rtc) return ESP_OK;
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), "rtc", "I2C");
    ESP_RETURN_ON_ERROR(i2c_master_probe(bsp_i2c_get_handle(), BOARD_RTC_ADDR, 100), "rtc", "RX8025T absent");
    i2c_device_config_t config = {.dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BOARD_RTC_ADDR, .scl_speed_hz = 100000};
    return i2c_master_bus_add_device(bsp_i2c_get_handle(), &config, &rtc);
}
static esp_err_t rtc_read_regs(uint8_t reg, uint8_t *data, size_t size)
{
    return i2c_master_transmit_receive(rtc, &reg, 1, data, size, 100);
}
static int unbcd(uint8_t n) { return (n & 15) <= 9 && (n >> 4) <= 9 ? (n >> 4) * 10 + (n & 15) : -1; }
static uint8_t bcd(int n) { return ((n / 10) << 4) | (n % 10); }
static bool valid_date(const struct tm *t)
{
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (!t || t->tm_year < 100 || t->tm_year > 199 || t->tm_mon < 0 || t->tm_mon > 11 ||
        t->tm_hour < 0 || t->tm_hour > 23 || t->tm_min < 0 || t->tm_min > 59 ||
        t->tm_sec < 0 || t->tm_sec > 59 || t->tm_wday < 0 || t->tm_wday > 6) return false;
    int max_day = days[t->tm_mon] + (t->tm_mon == 1 && (t->tm_year % 4) == 0);
    return t->tm_mday >= 1 && t->tm_mday <= max_day;
}

esp_err_t demo_rtc_read(struct tm *utc, bool *valid)
{
    if (!utc || !valid) return ESP_ERR_INVALID_ARG;
    *valid = false;
    ESP_RETURN_ON_ERROR(rtc_init(), "rtc", "init");
    uint8_t data[7], flags, seconds;
    /* RX8025T 是直接寄存器地址，不使用 RX8025SA 的 reg<<4 协议。 */
    for (int attempt = 0; attempt < 3; attempt++) {
        ESP_RETURN_ON_ERROR(rtc_read_regs(0x00, data, sizeof(data)), "rtc", "read time");
        ESP_RETURN_ON_ERROR(rtc_read_regs(0x00, &seconds, 1), "rtc", "read seconds");
        if (seconds == data[0]) break;
        if (attempt == 2) return ESP_ERR_TIMEOUT;
    }
    ESP_RETURN_ON_ERROR(rtc_read_regs(0x0e, &flags, 1), "rtc", "read flags");
    memset(utc, 0, sizeof(*utc));
    utc->tm_sec = unbcd(data[0] & 0x7f);
    utc->tm_min = unbcd(data[1] & 0x7f);
    utc->tm_hour = unbcd(data[2] & 0x3f);
    utc->tm_mday = unbcd(data[4] & 0x3f);
    utc->tm_mon = unbcd(data[5] & 0x1f) - 1;
    utc->tm_year = unbcd(data[6]) + 100;
    uint8_t week = data[3] & 0x7f;
    utc->tm_wday = week ? __builtin_ctz(week) : -1;
    *valid = !(flags & 0x02) && week && !(week & (week - 1)) && valid_date(utc);
    return ESP_OK;
}

esp_err_t demo_rtc_write(const struct tm *utc)
{
    if (!valid_date(utc)) return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(rtc_init(), "rtc", "init");
    uint8_t packet[] = {0x00, bcd(utc->tm_sec), bcd(utc->tm_min), bcd(utc->tm_hour),
        1u << utc->tm_wday, bcd(utc->tm_mday), bcd(utc->tm_mon + 1), bcd(utc->tm_year - 100)};
    ESP_RETURN_ON_ERROR(i2c_master_transmit(rtc, packet, sizeof(packet), 100), "rtc", "write");
    uint8_t flags;
    ESP_RETURN_ON_ERROR(rtc_read_regs(0x0e, &flags, 1), "rtc", "flags");
    uint8_t clear[] = {0x0e, flags & (uint8_t)~0x03};
    return i2c_master_transmit(rtc, clear, sizeof(clear), 100);
}

esp_err_t demo_rtc_show(char *result, size_t capacity)
{
    struct tm t;
    bool valid;
    ESP_RETURN_ON_ERROR(demo_rtc_read(&t, &valid), "rtc", "read");
    if (!valid) {
        snprintf(result, capacity, "RTC responded, but time is invalid or oscillator-loss flag is set. Call demo_rtc_write() with a valid UTC date.");
        return ESP_ERR_INVALID_STATE;
    }
    snprintf(result, capacity, "RX8025T: %04d-%02d-%02d %02d:%02d:%02d (stored as UTC by this example)",
        t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    return ESP_OK;
}

static bool uart_ready;
esp_err_t demo_rs485_init(void)
{
    if (uart_ready) return ESP_OK;
    uart_config_t cfg = {.baud_rate = CONFIG_DEMO_RS485_BAUD, .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE, .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE, .source_clk = UART_SCLK_DEFAULT};
    ESP_RETURN_ON_ERROR(uart_param_config(UART_NUM_1, &cfg), "rs485", "config");
    ESP_RETURN_ON_ERROR(uart_set_pin(UART_NUM_1, BOARD_RS485_TX, BOARD_RS485_RX,
        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE), "rs485", "pins");
    ESP_RETURN_ON_ERROR(uart_driver_install(UART_NUM_1, 2048, 0, 0, NULL, 0), "rs485", "driver");
    /* 板上电路自动控制收发方向，使用普通 UART，不另外占用 RTS。 */
    uart_ready = true;
    return ESP_OK;
}
esp_err_t demo_rs485_send(const void *data, size_t size)
{
    if (!data || !size || size > 2048) return ESP_ERR_INVALID_ARG;
    ESP_RETURN_ON_ERROR(demo_rs485_init(), "rs485", "init");
    if (uart_write_bytes(UART_NUM_1, data, size) != size) return ESP_FAIL;
    return uart_wait_tx_done(UART_NUM_1, pdMS_TO_TICKS(20000));
}
int demo_rs485_receive(void *data, size_t capacity, uint32_t timeout_ms)
{
    if (!data || !capacity || !uart_ready) return -1;
    return uart_read_bytes(UART_NUM_1, data, capacity, pdMS_TO_TICKS(timeout_ms));
}
esp_err_t demo_rs485_test(char *result, size_t capacity)
{
    extern bool serial_uart_in_use(void);
    if (serial_uart_in_use()) return ESP_ERR_INVALID_STATE;
    const char message[] = "Hello from JC1060 RS485\r\n";
    ESP_RETURN_ON_ERROR(demo_rs485_send(message, sizeof(message) - 1), "rs485", "send");
    uint8_t reply[64];
    int count = demo_rs485_receive(reply, sizeof(reply), 3000);
    if (count < 0) return ESP_FAIL;
    ESP_LOG_BUFFER_HEX("rs485_rx", reply, count);
    snprintf(result, capacity, "TX %u bytes; RX %d bytes within 3 s, %d baud 8N1. RX hex is in serial log.",
        (unsigned)sizeof(message) - 1, count, CONFIG_DEMO_RS485_BAUD);
    return count ? ESP_OK : ESP_ERR_TIMEOUT;
}

static atomic_bool button_enabled;
static TaskHandle_t button_handle;
void demo_button_pause(void) { atomic_store(&button_enabled, false); }

static void button_task(void *arg)
{
    int stable = 1, previous = 1, repeats = 0;
    unsigned clicks = 0;
    for (;;) {
        if (!atomic_load(&button_enabled)) {
            stable = previous = 1;
            repeats = 0;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        int level = gpio_get_level(BOARD_BOOT_GPIO);
        repeats = level == previous ? repeats + 1 : 0;
        previous = level;
        if (repeats >= 3) {
            repeats = 3;
            if (level != stable) {
                stable = level;
                if (!level) {
                    char text[80];
                    snprintf(text, sizeof(text), "BOOT button GPIO35: %u presses", ++clicks);
                    demo_post_status(text);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void demo_button_start(void)
{
    gpio_config_t cfg = {.pin_bit_mask = 1ULL << BOARD_BOOT_GPIO, .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE, .intr_type = GPIO_INTR_DISABLE};
    if (gpio_config(&cfg) == ESP_OK) {
        atomic_store(&button_enabled, true);
        if (!button_handle && xTaskCreate(button_task, "boot_key", 3072, NULL, 3, &button_handle) != pdPASS)
            demo_post_status("BOOT key task allocation failed");
    }
}
