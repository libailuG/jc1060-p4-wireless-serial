#include <stdio.h>
#include <stdatomic.h>
#include "app_registry.h"
#include "xiaozhi_service.h"
#include "platform.h"

static lv_obj_t *summary, *transcript, *buttons[5], *mode_hint;
static lv_timer_t *timer;
static atomic_int init_result;
static esp_err_t initialize(char *out, size_t capacity)
{
    esp_err_t e = xiaozhi_init();
    atomic_store(&init_result, e);
    snprintf(out, capacity, "%s", e == ESP_OK ? "小智已就绪，请连接。" : "小智初始化失败，请查看诊断。");
    return e;
}
static void enabled(lv_obj_t *b,bool on){
    if(on)lv_obj_remove_state(b,LV_STATE_DISABLED);else lv_obj_add_state(b,LV_STATE_DISABLED);
}
static bool can_start(const xiaozhi_snapshot_t *s){
    return s->available&&!s->held&&!s->continuous&&
        (s->state==XZ_READY||s->state==XZ_OFFLINE||s->state==XZ_ERROR);
}
static void update(lv_timer_t *unused)
{
    xiaozhi_snapshot_t s; xiaozhi_snapshot(&s);
    const char *message = s.available ? s.status : atomic_load(&init_result) == ESP_OK ? "正在初始化语音服务" : "初始化未完成，请稍后重新进入或查看诊断";
    lv_label_set_text_fmt(summary, "%s\n%s", message, s.activation);
    lv_label_set_text_fmt(transcript, "你：%s\n\n小智：%s", s.user, s.answer);
    enabled(buttons[0],can_start(&s));enabled(buttons[1],can_start(&s));enabled(buttons[2],can_start(&s));
    enabled(buttons[3],s.available&&s.held&&!s.continuous&&s.state==XZ_LISTENING);
    enabled(buttons[4],s.available);
    lv_label_set_text(mode_hint,s.continuous?
        "连续聊天中：说完稍停，回答结束后会继续听。点击停止可结束。":
        "连续聊天：点击一次，多轮对话。手动说话：点击开始，再点击发送语音。");
}
static void action(lv_event_t *e)
{
    xiaozhi_snapshot_t s;xiaozhi_snapshot(&s);
    unsigned op=(uintptr_t)lv_event_get_user_data(e);
    if(op<3&&!can_start(&s))return;
    switch (op) {
        case 0: xiaozhi_connect(); break;
        case 1: xiaozhi_continuous_start(); break;
        case 2: xiaozhi_hold(true); break;
        case 3: if(s.held&&!s.continuous&&s.state==XZ_LISTENING)xiaozhi_hold(false);break;
        default: xiaozhi_cancel(); break;
    }
    update(NULL);
}
static void open_chat(lv_obj_t *parent)
{
    summary = lv_label_create(parent); lv_obj_set_pos(summary, 16, 16); lv_obj_set_width(summary, 920);
    transcript = lv_label_create(parent); lv_obj_set_pos(transcript, 16, 236); lv_obj_set_width(transcript, 920);
    mode_hint=lv_label_create(parent);lv_obj_set_pos(mode_hint,16,174);lv_obj_set_width(mode_hint,920);
    const char *names[] = {"连接", "连续聊天", "手动说话", "发送语音", "停止"};
    for (unsigned i = 0; i < 5; i++) {
        lv_obj_t *b = lv_button_create(parent); lv_obj_set_pos(b, 16 + i * 186, 95); lv_obj_set_size(b, 174, 60);
        buttons[i]=b;
        lv_obj_add_event_cb(b, action, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *label = lv_label_create(b); lv_label_set_text(label, names[i]); lv_obj_center(label);
    }
    atomic_store(&init_result, ESP_OK);
    if (platform_submit("初始化小智", initialize) != ESP_OK) {
        atomic_store(&init_result, ESP_ERR_INVALID_STATE);
        demo_post_status("系统忙碌，请稍后重新进入小智。");
    }
    update(NULL); timer = lv_timer_create(update, 250, NULL);
}
static void close_chat(void)
{
    xiaozhi_leave();
    if (timer) lv_timer_delete(timer);
    timer = NULL;
}
const app_descriptor_t xiaozhi_app = {
    .id = "xiaozhi", .name = "小智", .description = "板载麦克风与扬声器语音对话",
    .open = open_chat, .close = close_chat,
};

