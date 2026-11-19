#include <math.h>
#include "app_registry.h"
#include "serial_service.h"
#include "platform.h"

static lv_obj_t *fields[10], *state, *keyboard, *chart, *repeat;
static lv_chart_series_t *series;
static lv_timer_t *timer;
static uint32_t cursor, lost;
static unsigned channel;
static int32_t minimum, maximum;
static serial_frame_t draft;
static void repeat_changed(lv_event_t *e)
{
    if (!lv_obj_has_state(repeat, LV_STATE_CHECKED)) serial_service_repeat(false);
}
static void focus(lv_event_t *e)
{
    lv_keyboard_set_textarea(keyboard, lv_event_get_target(e));
    lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(keyboard);
}
static void keyboard_done(lv_event_t *e)
{
    lv_keyboard_set_textarea(keyboard, NULL); lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
}
static void send_frame(lv_event_t *e)
{
    serial_frame_t next = draft;
    for (unsigned i = 0; i < 10; i++) if (!serial_value_parse(&next, i, lv_textarea_get_text(fields[i]))) {
        lv_label_set_text_fmt(state, "data%u 输入无效，未发送", i + 1); return;
    }
    draft = next;
    if (!serial_service_send(&draft, lv_obj_has_state(repeat, LV_STATE_CHECKED)))
        lv_label_set_text(state, "发送队列忙碌，请稍后重试");
}
static void update(lv_timer_t *unused)
{
    serial_status_t s; serial_service_snapshot(&s);
    char a[48], b[48];
    serial_value_format(&s.received, 2, a, sizeof(a), false);
    serial_value_format(&s.received, 3, b, sizeof(b), false);
    lv_label_set_text_fmt(state, "%s | TX %lu RX %lu | data3 %s  data4 %s\n丢弃字节 %lu  帧尾错误 %lu | %s",
        s.active ? "RS485 115200 8N1" : "串口初始化中", (unsigned long)s.tx, (unsigned long)s.rx,
        a, b, (unsigned long)s.discarded, (unsigned long)s.bad_tail, esp_err_to_name(s.error));
    serial_sample_t samples[32];
    size_t n = serial_service_read(&cursor, samples, 32, &lost);
    for (size_t i = 0; i < n; i++) {
        float y = samples[i].values[channel];
        int32_t point = LV_CHART_POINT_NONE;
        if (isfinite(y) && y > -10000000 && y < 10000000) {
            point = (int32_t)(y * 100);
            if (point < minimum) minimum = point;
            if (point > maximum) maximum = point;
        }
        lv_chart_set_next_value(chart, series, point);
    }
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, minimum - 100, maximum + 100);
}
static void choose_channel(lv_event_t *e)
{
    channel = lv_dropdown_get_selected(lv_event_get_target(e));
    serial_status_t s; serial_service_snapshot(&s);
    cursor = s.rx > 512 ? s.rx - 512 : 0; minimum = maximum = 0;
    lv_chart_set_all_value(chart, series, LV_CHART_POINT_NONE);
}
static void open_serial(lv_obj_t *parent)
{
    if (platform_busy()) {
        lv_obj_t *label = lv_label_create(parent);
        lv_label_set_text(label, "请等待当前硬件任务完成后重新进入。");
        return;
    }
    esp_err_t e = serial_service_init();
    if (e != ESP_OK) { demo_post_status("串口服务启动失败"); return; }
    serial_frame_init(&draft); serial_service_enable(true);
    for (unsigned i = 0; i < 10; i++) {
        int x = 16 + (i % 5) * 185, y = 12 + (i / 5) * 88;
        lv_obj_t *label = lv_label_create(parent); lv_obj_set_pos(label, x, y);
        lv_label_set_text_fmt(label, "data%u", i + 1);
        fields[i] = lv_textarea_create(parent); lv_obj_set_pos(fields[i], x, y + 25);
        lv_obj_set_size(fields[i], 170, 48); lv_textarea_set_one_line(fields[i], true);
        lv_textarea_set_max_length(fields[i], 32); lv_textarea_set_text(fields[i], "0");
        lv_obj_add_event_cb(fields[i], focus, LV_EVENT_CLICKED, NULL);
    }
    repeat = lv_switch_create(parent); lv_obj_set_pos(repeat, 16, 200);
    lv_obj_add_event_cb(repeat, repeat_changed, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_t *label = lv_label_create(parent); lv_obj_set_pos(label, 90, 205); lv_label_set_text(label, "持续发送（20ms）");
    lv_obj_t *button = lv_button_create(parent); lv_obj_set_pos(button, 380, 190); lv_obj_set_size(button, 200, 48);
    lv_obj_add_event_cb(button, send_frame, LV_EVENT_CLICKED, NULL);
    label = lv_label_create(button); lv_label_set_text(label, "发送 / 应用模式"); lv_obj_center(label);
    state = lv_label_create(parent); lv_obj_set_pos(state, 16, 250); lv_obj_set_width(state, 920);
    chart = lv_chart_create(parent); lv_obj_set_pos(chart, 16, 320); lv_obj_set_size(chart, 920, 240);
    lv_chart_set_point_count(chart, 512); lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, -10000, 10000);
    series = lv_chart_add_series(chart, lv_color_hex(0x2277dd), LV_CHART_AXIS_PRIMARY_Y);
    minimum = maximum = 0; channel = 0;
    serial_status_t snapshot; serial_service_snapshot(&snapshot);
    cursor = snapshot.rx > 512 ? snapshot.rx - 512 : 0; lost = 0;
    lv_obj_t *channels = lv_dropdown_create(parent);
    lv_obj_set_pos(channels, 630, 190); lv_obj_set_width(channels, 280);
    lv_dropdown_set_options(channels, "曲线 data3\n曲线 data4\n曲线 data5\n曲线 data6\n曲线 data7\n曲线 data8\n曲线 data9\n曲线 data10");
    lv_obj_add_event_cb(channels, choose_channel, LV_EVENT_VALUE_CHANGED, NULL);
    keyboard = lv_keyboard_create(parent); lv_obj_set_pos(keyboard, 0, 180); lv_obj_set_size(keyboard, 950, 280);
    lv_keyboard_set_mode(keyboard, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_add_event_cb(keyboard, keyboard_done, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(keyboard, keyboard_done, LV_EVENT_CANCEL, NULL);
    lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    update(NULL); timer = lv_timer_create(update, 50, NULL);
}
static void close_serial(void)
{
    serial_service_enable(false);
    if (timer) lv_timer_delete(timer);
    timer = NULL;
}
const app_descriptor_t serial_app = {
    .id = "serial", .name = "有线串口", .description = "RS485 结构体收发与 data3 曲线",
    .open = open_serial, .close = close_serial,
};
