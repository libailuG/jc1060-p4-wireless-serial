#pragma once
#include <stddef.h>
#include "lvgl.h"
/* Callbacks run in LVGL task; close releases app timers/resources.
 * Use platform_submit() for blocking work. Parent widgets are deleted by shell. */
typedef struct {
    const char *id;
    const char *name;
    const char *description;
    void (*open)(lv_obj_t *parent);
    void (*close)(void);
    bool fullscreen; /* App supplies its own return control. */
} app_descriptor_t;
extern const app_descriptor_t sample_app;
extern const app_descriptor_t counter_app, clock_app;
extern const app_descriptor_t weather_app;
extern const app_descriptor_t *const app_registry[];
extern const size_t app_registry_count;
extern const app_descriptor_t serial_app;
extern const app_descriptor_t xiaozhi_app;
extern const app_descriptor_t keys_app;
extern const app_descriptor_t bridge_app;
extern const app_descriptor_t wireless_app;
