#include "app_registry.h"
#include "esp_log.h"
static unsigned count;
static lv_obj_t *value;
static void increment(lv_event_t *event) { lv_label_set_text_fmt(value, "本次运行点击：%u", ++count); }
static void open_sample(lv_obj_t *parent)
{
    ESP_LOGI("sample", "enter");
    lv_obj_t *hint = lv_label_create(parent);
    lv_label_set_text(hint, "业务模块示例：返回首页后再次进入，计数保留；重启后清零。\n后续应用在这里创建页面，通过 platform.h 调用系统服务。");
    lv_obj_set_width(hint, 900); lv_obj_set_pos(hint, 16, 16);
    value = lv_label_create(parent); lv_obj_set_pos(value, 16, 100);
    lv_label_set_text_fmt(value, "本次运行点击：%u", count);
    lv_obj_t *button = lv_button_create(parent);
    lv_obj_set_pos(button, 16, 150); lv_obj_set_size(button, 220, 60);
    lv_obj_add_event_cb(button, increment, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label = lv_label_create(button); lv_label_set_text(label, "计数 +1"); lv_obj_center(label);
}
static void close_sample(void) { value = NULL; ESP_LOGI("sample", "exit"); }
const app_descriptor_t sample_app = {
    .id = "sample", .name = "示例应用", .description = "验证应用进入、退出与界面交互",
    .open = open_sample, .close = close_sample,
};
