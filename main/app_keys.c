#include <stdio.h>
#include <stdatomic.h>
#include "app_registry.h"
#include "platform.h"
#include "usb_service.h"
#include "ble_mouse.h"
#include "keymouse_service.h"
#include "class/hid/hid_device.h"

typedef struct {const char *title;uint8_t mods,key;} shortcut_t;
#define C KEYBOARD_MODIFIER_LEFTCTRL
#define S KEYBOARD_MODIFIER_LEFTSHIFT
#define A KEYBOARD_MODIFIER_LEFTALT
#define W KEYBOARD_MODIFIER_LEFTGUI
static const shortcut_t shortcuts[]={
 {"桌面向左",W|C,HID_KEY_ARROW_LEFT},{"桌面向右",W|C,HID_KEY_ARROW_RIGHT},
 {"显示桌面",W,HID_KEY_D},{"任务视图",W,HID_KEY_TAB},
 {"复制",C,HID_KEY_C},{"粘贴",C,HID_KEY_V},{"剪切",C,HID_KEY_X},{"全选",C,HID_KEY_A},
 {"撤销",C,HID_KEY_Z},{"重做",C,HID_KEY_Y},{"保存",C,HID_KEY_S},{"查找",C,HID_KEY_F},
 {"新建",C,HID_KEY_N},{"打开",C,HID_KEY_O},{"打印",C,HID_KEY_P},{"关闭标签",C,HID_KEY_W},
 {"新建标签",C,HID_KEY_T},{"恢复标签",C|S,HID_KEY_T},{"切换窗口",A,HID_KEY_TAB},{"取消",0,HID_KEY_ESCAPE}
};
static atomic_bool active;
static atomic_bool bluetooth=true;
static atomic_uint owner;
static atomic_int startup;
static lv_obj_t *status;
static lv_timer_t *timer;
static lv_obj_t *mode_button,*pair_button,*key_page,*control_page,*page_button;
extern void demo_request_app(const char *id);
static esp_err_t start(char *out,size_t size)
{
    bool ble=atomic_load(&bluetooth);
    keymouse_forward(false);
    esp_err_t e=ble?ble_mouse_init():usb_service_start();
    atomic_store(&startup,e);
    if(e==ESP_OK&&!ble) atomic_store(&owner,usb_service_set_owner(atomic_load(&active)));
    snprintf(out,size,"触屏快捷键：%s",esp_err_to_name(e));return e;
}
static void tick(lv_timer_t *unused)
{
    esp_err_t e=atomic_load(&startup);
    if(e==ESP_ERR_NOT_FINISHED)lv_obj_add_state(mode_button,LV_STATE_DISABLED);
    else lv_obj_remove_state(mode_button,LV_STATE_DISABLED);
    if(atomic_load(&bluetooth)&&e==ESP_OK)lv_obj_remove_state(pair_button,LV_STATE_DISABLED);
    else lv_obj_add_state(pair_button,LV_STATE_DISABLED);
    if(e==ESP_ERR_NOT_FINISHED){lv_label_set_text(status,"正在启动，请稍候…");return;}
    if(atomic_load(&bluetooth)){
        ble_mouse_snapshot_t b;ble_mouse_snapshot(&b);
        lv_label_set_text_fmt(status,"TouchDesk KeyMouse｜键盘 %s｜鼠标 %s｜音量 %s\n%s",
            b.keyboard_ready?"就绪":"未就绪",b.ready?"就绪":"未就绪",b.consumer_ready?"就绪":"未就绪",
            e!=ESP_OK?"启动失败，请重新启动":b.connected?"蓝牙已连接":b.pairing?"请在电脑蓝牙设置添加设备":"等待回连，新电脑请开启配对");return;
    }
    lv_label_set_text_fmt(status,"%s\n使用高速 OTG 接口连接电脑；COM13 调试口不发送键鼠。",
        e!=ESP_OK ? "USB 启动失败，请查看诊断后重新进入" : usb_service_online() ? "电脑已连接，点击按钮发送" : "等待电脑连接或服务启动");
}
static void click(lv_event_t *event)
{
    const shortcut_t *key=lv_event_get_user_data(event);
    if(atomic_load(&startup)!=ESP_OK || !(atomic_load(&bluetooth)?ble_keyboard_click(key->mods,key->key):usb_service_click(atomic_load(&owner),usb_service_session(),key->mods,key->key)))
        lv_label_set_text(status,"电脑未连接或队列忙碌，未发送。");
}
static void extra(lv_event_t *event)
{
    unsigned op=(uintptr_t)lv_event_get_user_data(event),o=atomic_load(&owner),s=usb_service_session();
    bool ok=false;
    if(atomic_load(&bluetooth)){
        if(op==8){ble_keyboard_usb_gone();ble_mouse_usb_gone();ble_consumer_release();return;}
        if(atomic_load(&startup)==ESP_OK&&op<3)ok=ble_consumer_click(op==0?0xea:op==1?0xe9:0xe2);
        if(atomic_load(&startup)==ESP_OK&&op>=3)ok=ble_mouse_action(op==3,op==4?-20:op==5?20:0,op==6?-20:op==7?20:0);
        if(!ok)lv_label_set_text(status,"蓝牙未就绪或队列忙碌，未发送。");
        return;
    }
    if(op==0)ok=usb_service_volume(o,s,-1);
    else if(op==1)ok=usb_service_volume(o,s,1);
    else if(op==2)ok=usb_service_mute(o,s);
    else if(op==3)ok=usb_service_mouse_click(o,s);
    else if(op<8)ok=usb_service_mouse(o,s,0,op==4?-20:op==5?20:0,op==6?-20:op==7?20:0,0);
    else {usb_service_cancel();ok=true;}
    if(!ok)lv_label_set_text(status,"电脑未连接或队列忙碌，未发送。");
}
static lv_obj_t *button(lv_obj_t *parent,int x,int y,int width,const char *title,lv_event_cb_t cb,void *data)
{
    lv_obj_t *b=lv_button_create(parent);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,width,44);
    lv_obj_add_event_cb(b,cb,LV_EVENT_CLICKED,data);
    lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,title);lv_obj_center(l);return b;
}
static void restart(void){
    atomic_store(&startup,ESP_ERR_NOT_FINISHED);
    if(platform_submit("启动触屏快捷键",start)!=ESP_OK)atomic_store(&startup,ESP_ERR_INVALID_STATE);
}
static void change_mode(lv_event_t *event){
    if(atomic_load(&startup)==ESP_ERR_NOT_FINISHED)return;
    usb_service_set_owner(false);ble_keyboard_usb_gone();ble_mouse_usb_gone();ble_consumer_release();
    bool ble=!atomic_load(&bluetooth);atomic_store(&bluetooth,ble);
    lv_label_set_text(lv_obj_get_child(mode_button,0),ble?"切换到 USB":"切换到蓝牙");
    restart();tick(NULL);
}
static void pair(lv_event_t *event){if(atomic_load(&bluetooth)&&atomic_load(&startup)==ESP_OK)ble_mouse_pair();}
static void retry(lv_event_t *event){if(atomic_load(&startup)!=ESP_ERR_NOT_FINISHED){restart();tick(NULL);}}
static void home(lv_event_t *event){demo_request_app("home");}
static void switch_page(lv_event_t *event){
    bool controls=lv_obj_has_flag(control_page,LV_OBJ_FLAG_HIDDEN);
    if(controls){lv_obj_add_flag(key_page,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(control_page,LV_OBJ_FLAG_HIDDEN);}
    else {lv_obj_add_flag(control_page,LV_OBJ_FLAG_HIDDEN);lv_obj_remove_flag(key_page,LV_OBJ_FLAG_HIDDEN);}
    lv_label_set_text(lv_obj_get_child(page_button,0),controls?"返回快捷键":"鼠标 / 音量");
}
static lv_obj_t *page(lv_obj_t *parent){
    lv_obj_t *p=lv_obj_create(parent);lv_obj_remove_style_all(p);
    lv_obj_set_pos(p,0,176);lv_obj_set_size(p,1024,424);lv_obj_remove_flag(p,LV_OBJ_FLAG_SCROLLABLE);return p;
}
static void large_button(lv_obj_t *parent,int x,int y,int w,int h,const char *title,lv_event_cb_t cb,void *data){
    lv_obj_t *b=button(parent,x,y,w,title,cb,data);lv_obj_set_height(b,h);
    lv_obj_set_style_radius(b,16,0);
    /* Scale the existing CJK glyphs together with Latin text. */
    lv_obj_set_style_transform_scale(lv_obj_get_child(b,0),384,0);
}
static void open_keys(lv_obj_t *parent)
{
    atomic_store(&active,true);
    lv_obj_t *root=lv_obj_create(parent);lv_obj_remove_style_all(root);
    lv_obj_set_size(root,1024,600);lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    button(root,16,12,120,"返回桌面",home,NULL);
    mode_button=button(root,152,12,200,atomic_load(&bluetooth)?"切换到 USB":"切换到蓝牙",change_mode,NULL);
    pair_button=button(root,368,12,248,"开启配对（120 秒）",pair,NULL);
    button(root,632,12,160,"重新启动",retry,NULL);
    page_button=button(root,808,12,200,"鼠标 / 音量",switch_page,NULL);
    status=lv_label_create(root);lv_obj_set_pos(status,20,76);lv_obj_set_width(status,980);
    key_page=page(root);control_page=page(root);lv_obj_add_flag(control_page,LV_OBJ_FLAG_HIDDEN);
    for(unsigned i=0;i<sizeof(shortcuts)/sizeof(shortcuts[0]);i++)
        large_button(key_page,16+(i%4)*250,4+(i/4)*82,240,72,shortcuts[i].title,click,(void*)&shortcuts[i]);
    const char *names[]={"音量减","音量加","静音","鼠标左键","鼠标左移","鼠标右移","鼠标上移","鼠标下移","释放全部"};
    for(unsigned i=0;i<9;i++)
        large_button(control_page,16+(i%3)*334,4+(i/3)*136,324,124,names[i],extra,(void*)(uintptr_t)i);
    restart();tick(NULL);timer=lv_timer_create(tick,500,NULL);
}
static void close_keys(void)
{
    atomic_store(&active,false);
    usb_service_set_owner(false);
    ble_keyboard_usb_gone();ble_mouse_usb_gone();ble_consumer_release();
    if(timer) lv_timer_delete(timer);
    timer=NULL;status=NULL;
}
const app_descriptor_t keys_app={.id="keysim",.name="触屏快捷键",.description="USB / 蓝牙快捷键和鼠标控制",.open=open_keys,.close=close_keys,.fullscreen=true};

