#pragma once
#include "lvgl.h"
#include "esp_err.h"
#define APP_CAP_FULLSCREEN 2u
#define APP_CAP_SERIAL 256u
#define APP_CAP_WIRELESS 512u
#define APP_MAX_TIMERS 8
struct app_ctx;
typedef struct app_ctx app_ctx_t;
typedef struct {const char *id,*title,*subtitle;unsigned caps;size_t state_size;esp_err_t (*create)(app_ctx_t*,lv_obj_t*);void (*destroy)(app_ctx_t*);bool (*back)(app_ctx_t*);} app_desc_t;
struct app_ctx {const app_desc_t *desc;void *state;lv_obj_t *root;bool active;lv_timer_t *timers[APP_MAX_TIMERS];};
lv_timer_t *app_timer_create(app_ctx_t *ctx,lv_timer_cb_t cb,uint32_t period);
void app_request(const app_desc_t *desc);
