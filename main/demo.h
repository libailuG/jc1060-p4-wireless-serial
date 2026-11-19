#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "esp_err.h"

/* 这些阻塞调用由单一 worker 串行执行。迁入多任务项目时请自行加互斥锁。 */
typedef esp_err_t (*demo_action_fn)(char *result, size_t capacity);
void demo_ui_start(void);
esp_err_t demo_request_wifi_scan(void); /* 可从 USB 调试任务投递，不访问 LVGL */
void demo_console_start(void);
void demo_post_status(const char *text); /* 可从任意任务调用；不直接操作 LVGL */
esp_err_t demo_storage_test(char *, size_t);
esp_err_t demo_nvs_test(char *, size_t);
esp_err_t demo_audio_tone(char *, size_t);
esp_err_t demo_audio_record_play(char *, size_t);
esp_err_t demo_camera_capture(char *, size_t);
esp_err_t demo_wifi_scan(char *, size_t);
esp_err_t demo_wifi_connect(char *, size_t);
esp_err_t demo_ethernet_start(char *, size_t);
esp_err_t demo_network_init(void);
void demo_wifi_status(char *out, size_t capacity);
unsigned demo_wifi_ap_snapshot(char names[8][33]);
esp_err_t demo_i2c_scan(char *, size_t);
esp_err_t demo_rtc_show(char *, size_t);
esp_err_t demo_rtc_read(struct tm *utc, bool *valid);
esp_err_t demo_rtc_write(const struct tm *utc);
esp_err_t demo_rs485_test(char *, size_t);
esp_err_t demo_rs485_init(void);
esp_err_t demo_rs485_send(const void *data, size_t size);
int demo_rs485_receive(void *data, size_t capacity, uint32_t timeout_ms);
esp_err_t demo_usb_start(char *, size_t);
void demo_button_start(void);
void demo_button_pause(void); /* GPIO35 将交给 Ethernet TXD1 */
bool demo_wifi_connected(void);
esp_err_t demo_usb_stop(void);
esp_err_t demo_wifi_wireless_mode(bool enabled);
void demo_request_app(const char *id);
