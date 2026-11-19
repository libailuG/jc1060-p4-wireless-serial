#include <stdio.h>
#include <stdatomic.h>
#include "app_registry.h"
#include "platform.h"
#include "keymouse_service.h"
#include "ble_mouse.h"

static atomic_bool active;
static atomic_int start_result;
static lv_obj_t *summary;
static lv_timer_t *timer;
esp_err_t bridge_start(char *out,size_t size)
{
    esp_err_t e=demo_usb_start(out,size);
    if(e==ESP_OK)e=keymouse_init();
    if(e==ESP_OK)e=keymouse_host_start();
    keymouse_forward(e==ESP_OK && atomic_load(&active));
    atomic_store(&start_result,e);
    snprintf(out,size,"USB → BLE 键鼠桥接：%s",esp_err_to_name(e));return e;
}

static void tick(lv_timer_t *unused)
{
    keymouse_snapshot_t u;keymouse_snapshot(&u);
    ble_mouse_snapshot_t b;ble_mouse_snapshot(&b);
    esp_err_t e=atomic_load(&start_result);
    lv_label_set_text_fmt(summary,
        "%s\n蓝牙名称：TouchDesk KeyMouse\nUSB 接口 %u（键盘 %u）｜鼠标报告 %u｜键盘报告 %u\n蓝牙：%s｜加密 %s｜鼠标 %s｜键盘 %s\n已转发 鼠标 %u / 键盘 %u｜失败 %u / %u",
        e==ESP_OK ? "将 USB 键鼠插入高速 OTG 口，再在电脑蓝牙设置中配对。" : e==ESP_ERR_NOT_FINISHED ? "正在启动 USB 和蓝牙服务…" : "桥接启动失败，请重试或查看诊断。",
        u.interfaces,u.key_interfaces,u.reports,u.key_reports,
        b.connected?"已连接":b.advertising?"等待配对":"未连接",b.encrypted?"是":"否",
        b.ready?"就绪":"未就绪",b.keyboard_ready?"就绪":"未就绪",b.sent,b.key_sent,b.failed,b.key_failed);
}
static void pair(lv_event_t *e){ble_mouse_pair();}
static void retry(lv_event_t *e)
{
    atomic_store(&start_result,ESP_ERR_NOT_FINISHED);
    if(platform_submit("启动键鼠桥接",bridge_start)!=ESP_OK) {
        atomic_store(&start_result,ESP_ERR_INVALID_STATE);
        demo_post_status("系统忙碌，桥接未启动，请重试。");
    }
}
static void open_bridge(lv_obj_t *parent)
{
    atomic_store(&active,true);
    summary=lv_label_create(parent);lv_obj_set_pos(summary,16,20);lv_obj_set_width(summary,920);
    const char *names[]={"开启配对（120 秒）","重新启动"};
    for(unsigned i=0;i<2;i++){
        lv_obj_t *b=lv_button_create(parent);lv_obj_set_pos(b,16+i*330,240);lv_obj_set_size(b,300,54);
        lv_obj_add_event_cb(b,i?retry:pair,LV_EVENT_CLICKED,NULL);
        lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,names[i]);lv_obj_center(l);
    }
    lv_obj_t *hint=lv_label_create(parent);lv_obj_set_pos(hint,16,330);lv_obj_set_width(hint,920);
    lv_label_set_text(hint,"支持一套标准 USB 键盘和鼠标，可通过 USB Hub 接入。\n退出应用会停止转发并释放按键；配对记录保留。\nCOM13 为调试口；本应用不需要电脑通过 USB 连接板子。");
    retry(NULL);tick(NULL);timer=lv_timer_create(tick,500,NULL);
}
static void close_bridge(void)
{
    atomic_store(&active,false);keymouse_forward(false);
    if(timer)lv_timer_delete(timer);
    timer=NULL;summary=NULL;
}
const app_descriptor_t bridge_app={.id="keymouse",.name="键鼠蓝牙桥接",.description="USB 键鼠通过蓝牙连接电脑",.open=open_bridge,.close=close_bridge};


