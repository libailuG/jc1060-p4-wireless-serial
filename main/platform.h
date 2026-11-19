#pragma once
#include "demo.h"

#define PLATFORM_VERSION "0.6.0-dev"
typedef struct {
    uint8_t brightness;
    uint16_t idle_seconds; /* 0 = never dim; touch restores brightness */
    bool auto_connect;
    bool boot_sample;
    char device_name[49];
    char ssid[33];
    char password[64];
} platform_config_t;

/* Initialize after NVS. Tasks share one serialized hardware worker. */
esp_err_t platform_init(void);
esp_err_t platform_submit(const char *title, demo_action_fn run);
bool platform_busy(void);
void platform_config_get(platform_config_t *out);
esp_err_t platform_config_set(const platform_config_t *config);
esp_err_t platform_config_save(char *result, size_t capacity);
esp_err_t platform_config_reset(char *result, size_t capacity);
/* Thread-safe bounded operation history. Passwords must never be logged. */
void platform_history(char *out, size_t capacity);
void platform_latest(char *out, size_t capacity);




