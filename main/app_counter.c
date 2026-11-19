#include <stdint.h>
#include <stdio.h>
#include "app_registry.h"
#include "platform.h"
#include "nvs.h"

static int32_t count;
static int32_t pending_save;
static bool loaded;
static lv_obj_t *value;

static esp_err_t save_count(char *result, size_t capacity)
{
    nvs_handle_t handle;
    int32_t saved = pending_save;
    esp_err_t ret = nvs_open("counter_p4", NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_set_i32(handle, "value", saved);
        if (ret == ESP_OK) ret = nvs_commit(handle);
        nvs_close(handle);
    }
    snprintf(result, capacity, ret == ESP_OK ? "计数已保存：%ld" : "计数保存失败：%ld", (long)saved);
    return ret;
}

static void action(lv_event_t *event)
{
    if (platform_busy()) return;
    intptr_t delta = (intptr_t)lv_event_get_user_data(event);
    if (delta == 2) {
        pending_save = count;
        if (platform_submit("保存计数", save_count) != ESP_OK)
            demo_post_status("系统忙碌，计数未保存，请重试。");
        return;
    }
    if ((delta > 0 && count == INT32_MAX) || (delta < 0 && count == INT32_MIN)) return;
    count += delta;
    lv_label_set_text_fmt(value, "%ld", (long)count);
}

static void open_counter(lv_obj_t *parent)
{
    /* NVS reads only at app entry; writes run on the system worker. */
    nvs_handle_t handle;
    if (!loaded && nvs_open("counter_p4", NVS_READONLY, &handle) == ESP_OK) {
        int32_t saved;
        if (nvs_get_i32(handle, "value", &saved) == ESP_OK) count = saved;
        nvs_close(handle);
    }
    loaded = true;
    value = lv_label_create(parent);
    lv_obj_set_pos(value, 24, 24);
    lv_label_set_text_fmt(value, "%ld", (long)count);
    const char *names[] = {"减一", "加一", "保存计数"};
    const intptr_t operations[] = {-1, 1, 2};
    for (unsigned i = 0; i < 3; i++) {
        lv_obj_t *button = lv_button_create(parent);
        lv_obj_set_pos(button, 24 + i * 280, 100);
        lv_obj_set_size(button, 250, 60);
        lv_obj_add_event_cb(button, action, LV_EVENT_CLICKED, (void *)operations[i]);
        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text(label, names[i]); lv_obj_center(label);
    }
}
static void close_counter(void) { value = NULL; }
const app_descriptor_t counter_app = {
    .id = "counter", .name = "计数器", .description = "加减计数，手动保存后断电保留",
    .open = open_counter, .close = close_counter,
};
