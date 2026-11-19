#include <string.h>
#include "demo.h"
#include "c6_update.h"
#include "esp_wifi.h"
#include "bsp/esp-bsp.h"
#include "serial_frame.h"
extern esp_err_t wireless_console_start(char*,size_t);
extern void wireless_console_ui(unsigned);
extern void wireless_console_report(void);
extern void wireless_console_send(const serial_frame_t*);
#include "platform.h"
#include "xiaozhi_service.h"
#include "usb_service.h"
#include "keymouse_service.h"
#include "ble_mouse.h"
extern esp_err_t bridge_start(char *,size_t);
#include <stdio.h>
#include "esp_log.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static esp_err_t chat_init(char *out, size_t size)
{
    esp_err_t e = xiaozhi_init(); snprintf(out, size, "小智服务：%s", esp_err_to_name(e)); return e;
}

static esp_err_t hid_start_action(char *out, size_t size)
{
    esp_err_t e=usb_service_start();snprintf(out,size,"USB HID start: %s",esp_err_to_name(e));return e;
}
static esp_err_t hid_stop_action(char *out, size_t size)
{
    esp_err_t e=usb_service_stop();snprintf(out,size,"USB HID stop: %s",esp_err_to_name(e));return e;
}
static void console_task(void *arg)
{
    char command[160];
    size_t used = 0;
    for (;;) {
        char ch;
        if (usb_serial_jtag_read_bytes(&ch, 1, pdMS_TO_TICKS(100)) != 1) continue;
        if (ch == '\r' || ch == '\n') {
            if (!used) continue;
            command[used] = 0;
            if (!strncmp(command,"c6 stage ",9)) {
                ESP_LOGI("console","stage: %s",esp_err_to_name(c6_stage_command(command)));
            } else if (!strncmp(command,"c6 data ",8)) {
                c6_stage_data(command);
            } else if (!strcmp(command,"c6 ota")) {
                platform_submit("更新板载 C6",c6_update);
            } else if (!strcmp(command,"wifi mac")) {
                uint8_t m[6];if(esp_wifi_get_mac(WIFI_IF_STA,m)==ESP_OK)ESP_LOGI("console","RADIO_MAC %02x:%02x:%02x:%02x:%02x:%02x",m[0],m[1],m[2],m[3],m[4],m[5]);
            } else if (!strncmp(command,"app ui ",7)) {
                demo_request_app(command+7);
            } else if (!strcmp(command,"now ui")) {
                demo_request_app("wireless");
            } else if (!strcmp(command,"now start")) {
                platform_submit("启动 ESP-NOW",wireless_console_start);
            } else if (!strcmp(command,"now stats")) {
                if(bsp_display_lock(1000)){wireless_console_report();bsp_display_unlock();}
            } else if (!strncmp(command,"now view ",9)) {
                unsigned v=!strcmp(command+9,"rx")?1:!strcmp(command+9,"curve")?2:0;
                if(bsp_display_lock(1000)){wireless_console_ui(v);bsp_display_unlock();}
            } else if (!strncmp(command,"now send ",9)&&strlen(command+9)==96) {
                uint8_t bytes[48];bool valid=true;
                for(unsigned i=0;i<48;i++){unsigned x;if(sscanf(command+9+2*i,"%2x",&x)!=1){valid=false;break;}bytes[i]=x;}
                serial_frame_t f;
                if(valid&&serial_frame_decode(bytes,&f)&&bsp_display_lock(1000)){wireless_console_send(&f);bsp_display_unlock();}
            } else if (!strcmp(command, "scan")) {
                esp_err_t ret = demo_request_wifi_scan();
                ESP_LOGI("console", "scan request: %s", esp_err_to_name(ret));
            } else if (!strcmp(command, "connect")) {
                esp_err_t ret = platform_submit("连接已保存 Wi-Fi", demo_wifi_connect);
                ESP_LOGI("console", "connect request: %s", esp_err_to_name(ret));
            } else if (!strcmp(command, "tone")) {
                platform_submit("板载扬声器测试", demo_audio_tone);
            } else if (!strcmp(command, "record")) {
                platform_submit("板载录音回放", demo_audio_record_play);
            } else if (!strcmp(command, "chat init")) {
                platform_submit("初始化小智", chat_init);
            } else if (!strcmp(command, "hid start")) {
                platform_submit("USB HID 启动",hid_start_action);
            } else if (!strcmp(command, "hid stop")) {
                platform_submit("USB HID 停止",hid_stop_action);
            } else if (!strcmp(command,"usb host")) {
                platform_submit("USB Host 启动",demo_usb_start);
            } else if (!strcmp(command,"bridge start")) {
                platform_submit("启动键鼠桥接",bridge_start);
            } else if (!strcmp(command,"bridge pair")) {
                ble_mouse_pair();
            } else if (!strcmp(command,"bridge status")) {
                keymouse_poll();
            } else if (xiaozhi_console(command)) {
                /* The service validates commands; no credentials are printed. */
            } else {
                ESP_LOGI("console", "Command: scan (same worker as the touchscreen button)");
            }
            used = 0;
        } else if (used < sizeof(command) - 1) command[used++] = ch;
        else used = 0;
    }
}

void demo_console_start(void)
{
    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t cfg = {.rx_buffer_size = 16384, .tx_buffer_size = 1024};
        esp_err_t ret = usb_serial_jtag_driver_install(&cfg);
        if (ret != ESP_OK) { ESP_LOGW("console", "USB RX unavailable: %s", esp_err_to_name(ret)); return; }
    }
    if (xTaskCreate(console_task, "usb_commands", 12288, NULL, 3, NULL) != pdPASS)
        ESP_LOGE("console", "Cannot create USB command task");
}








