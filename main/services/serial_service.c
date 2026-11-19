#include <stdatomic.h>
#include "serial_service.h"
#include "demo.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"

typedef struct { serial_frame_t frame; bool continuous; } send_t;
static QueueHandle_t outgoing;
static atomic_bool wanted;
static atomic_bool repeating;
static portMUX_TYPE guard = portMUX_INITIALIZER_UNLOCKED;
static serial_status_t status;
static serial_history_t *history;
size_t serial_service_read(uint32_t *cursor, serial_sample_t *out, size_t capacity, uint32_t *lost)
{
    if (!history) return 0;
    if (capacity > 32) capacity = 32;
    portENTER_CRITICAL(&guard);
    size_t n = serial_history_read(history, cursor, out, capacity, lost);
    portEXIT_CRITICAL(&guard);
    return n;
}
bool serial_uart_in_use(void)
{
    serial_status_t s; serial_service_snapshot(&s);
    return atomic_load(&wanted) || s.active;
}
static void received(const serial_frame_t *frame, void *unused)
{
    int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&guard); status.received = *frame; status.rx++;
    serial_history_push_timed(history, frame, now); portEXIT_CRITICAL(&guard);
}
void serial_service_snapshot(serial_status_t *out)
{
    portENTER_CRITICAL(&guard); *out = status; portEXIT_CRITICAL(&guard);
}
void serial_service_enable(bool enabled) { if (!enabled) atomic_store(&repeating, false); atomic_store(&wanted, enabled); }
void serial_service_repeat(bool enabled) { atomic_store(&repeating, enabled); }
bool serial_service_send(const serial_frame_t *frame, bool continuous)
{
    if (!outgoing || !atomic_load(&wanted)) return false;
    send_t item = {.frame = *frame, .continuous = continuous};
    if (xQueueSend(outgoing, &item, 0) != pdTRUE) return false;
    atomic_store(&repeating, continuous);
    return true;
}
static void worker(void *unused)
{
    serial_parser_t parser = {0}; send_t item = {0};
    bool active = false, continuous = false;
    TickType_t last = 0;
    for (;;) {
        bool enable = atomic_load(&wanted);
        if (enable && !active) {
            esp_err_t e = demo_rs485_init();
            portENTER_CRITICAL(&guard); status.error = e; status.active = e == ESP_OK; portEXIT_CRITICAL(&guard);
            active = e == ESP_OK; parser = (serial_parser_t){0};
            if (active) uart_flush_input(UART_NUM_1);
        }
        if (!enable) {
            active = continuous = false;
            while (xQueueReceive(outgoing, &item, 0) == pdTRUE) {}
            portENTER_CRITICAL(&guard); status.active = status.continuous = false; portEXIT_CRITICAL(&guard);
            vTaskDelay(pdMS_TO_TICKS(20)); continue;
        }
        if (!active) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }
        bool send = xQueueReceive(outgoing, &item, 0) == pdTRUE;
        continuous = atomic_load(&repeating);
        TickType_t now = xTaskGetTickCount();
        if (send || (continuous && now - last >= pdMS_TO_TICKS(20))) {
            uint8_t bytes[SERIAL_FRAME_SIZE]; serial_frame_encode(&item.frame, bytes);
            /* No flow control; a 48-byte frame takes about 4.2 ms. */
            int written = uart_write_bytes(UART_NUM_1, bytes, sizeof(bytes));
            portENTER_CRITICAL(&guard);
            if (written == sizeof(bytes)) status.tx++;
            else status.error = ESP_ERR_TIMEOUT;
            status.continuous = continuous;
            portEXIT_CRITICAL(&guard); last = now;
        }
        uint8_t bytes[256]; int n = demo_rs485_receive(bytes, sizeof(bytes), 10);
        if (n > 0) serial_parser_feed(&parser, bytes, n, received, NULL);
        portENTER_CRITICAL(&guard); status.discarded = parser.discarded; status.bad_tail = parser.bad_tail; portEXIT_CRITICAL(&guard);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
esp_err_t serial_service_init(void)
{
    if (outgoing) return ESP_OK;
    history = heap_caps_calloc(1, sizeof(*history), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!history) return ESP_ERR_NO_MEM;
    outgoing = xQueueCreate(8, sizeof(send_t));
    if (!outgoing) { heap_caps_free(history); history = NULL; return ESP_ERR_NO_MEM; }
    if (xTaskCreate(worker, "serial_app", 4096, NULL, 4, NULL) != pdPASS) {
        vQueueDelete(outgoing); outgoing = NULL; heap_caps_free(history); history = NULL; return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
