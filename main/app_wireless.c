#include "app_registry.h"
#include "wireless/app.h"
#include "wireless/ui.h"
#include "wireless/weather_style.h"
#include "wireless/serial_transport.h"
#include "wireless/radio.h"
#include "platform.h"
#include "weather.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdatomic.h>
extern const app_desc_t app_wireless;
static app_ctx_t ctx;
static atomic_bool desired;
static atomic_bool pending;
static lv_timer_t *service_timer;
LV_FONT_DECLARE(wireless_ui_20);
static const weather_palette_t palette={0xeaf4ff,0x173a5b,0x526b83,0xa8c2df,0xe3effc,0x087bff};
const weather_palette_t *weather_palette(unsigned theme){return &palette;}
unsigned ui_theme(void){return 1;}
bool ui_dark_theme(void){return false;}
lv_color_t ui_background(void){return lv_color_hex(palette.bg);}
lv_color_t ui_foreground(void){return lv_color_hex(palette.ink);}
lv_color_t ui_surface(void){return lv_color_hex(palette.surface);}
lv_color_t ui_accent_foreground(void){return lv_color_white();}
lv_obj_t *ui_button(lv_obj_t *parent,const char *text,int x,int y,int w,int h,lv_event_cb_t cb,void *arg)
{
 lv_obj_t *b=lv_button_create(parent);lv_obj_set_pos(b,x,y);lv_obj_set_size(b,w,h);
 lv_obj_set_style_bg_color(b,ui_surface(),0);lv_obj_set_style_text_color(b,ui_foreground(),0);
 lv_obj_set_style_radius(b,12,0);lv_obj_set_style_text_font(b,&wireless_ui_20,0);
 lv_obj_set_style_border_width(b,1,0);lv_obj_set_style_border_color(b,lv_color_hex(0xa8c2df),0);lv_obj_set_style_shadow_width(b,0,0);
 lv_obj_t *l=lv_label_create(b);lv_label_set_text(l,text);lv_obj_set_width(l,w-12);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);lv_obj_center(l);
 if(cb)lv_obj_add_event_cb(b,cb,LV_EVENT_ALL,arg);
 return b;
}
lv_timer_t *app_timer_create(app_ctx_t *c,lv_timer_cb_t cb,uint32_t period)
{
 for(unsigned i=0;i<APP_MAX_TIMERS;i++)if(!c->timers[i])return c->timers[i]=lv_timer_create(cb,period,c);
 return NULL;
}
extern void demo_request_app(const char *id);
void app_request(const app_desc_t *desc){demo_request_app(desc?desc->id:"home");}
esp_err_t wireless_start_action(char *out,size_t capacity)
{
 esp_err_t e=demo_wifi_wireless_mode(true);
 if(e==ESP_OK){serial_transport_start();e=td_radio_enable(true);}
 if(e!=ESP_OK||!atomic_load(&desired)){td_radio_enable(false);demo_wifi_wireless_mode(false);}
 snprintf(out,capacity,"ESP-NOW: %s",esp_err_to_name(e));return e;
}
static esp_err_t stop(char *out,size_t capacity)
{
 esp_err_t e=td_radio_enable(false);esp_err_t net=demo_wifi_wireless_mode(false);
 snprintf(out,capacity,"ESP-NOW stop: %s",esp_err_to_name(e));return e==ESP_OK?net:e;
}
static esp_err_t reconcile(char *out,size_t capacity)
{
 bool target=atomic_load(&desired);
 esp_err_t e=target?wireless_start_action(out,capacity):stop(out,capacity);
 if(target!=atomic_load(&desired))atomic_store(&pending,true);
 return e;
}
static void service_tick(lv_timer_t *timer)
{
 if(!atomic_load(&pending)||platform_busy())return;
 atomic_store(&pending,false);
 if(platform_submit("切换 ESP-NOW",reconcile)!=ESP_OK)atomic_store(&pending,true);
}
static void open_wireless(lv_obj_t *parent)
{
 if(!service_timer)service_timer=lv_timer_create(service_tick,200,NULL);
 atomic_store(&desired,true);serial_transport_start();
 ctx=(app_ctx_t){.desc=&app_wireless,.active=true};ctx.root=lv_obj_create(parent);
 lv_obj_remove_style_all(ctx.root);lv_obj_set_size(ctx.root,1024,600);lv_obj_set_pos(ctx.root,0,0);lv_obj_remove_flag(ctx.root,LV_OBJ_FLAG_SCROLLABLE);
 ctx.state=heap_caps_calloc(1,app_wireless.state_size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!ctx.state){demo_post_status("无线页面内存不足");demo_request_app("home");return;}
 esp_err_t e=app_wireless.create(&ctx,ctx.root);

 if(e!=ESP_OK){demo_post_status("无线页面创建失败");demo_request_app("home");}
 atomic_store(&pending,true);
}
static void close_wireless(void)
{
 atomic_store(&desired,false);ctx.active=false;
 for(unsigned i=0;i<APP_MAX_TIMERS;i++)if(ctx.timers[i])lv_timer_delete(ctx.timers[i]);
 if(ctx.state){app_wireless.destroy(&ctx);free(ctx.state);ctx.state=NULL;}
 atomic_store(&pending,true);
}
const app_descriptor_t wireless_app={.id="wireless",.name="ESP-NOW 无线串口",.description="发送与接收界面",.open=open_wireless,.close=close_wireless,.fullscreen=true};

lv_obj_t *ui_label(lv_obj_t *parent,const char *text,int x,int y,int width)
{
 lv_obj_t *l=lv_label_create(parent);lv_obj_set_pos(l,x,y);lv_obj_set_width(l,width);
 lv_obj_set_style_text_font(l,demo_text_font(),0);lv_obj_set_style_text_color(l,ui_foreground(),0);lv_label_set_text(l,text);return l;
}

esp_err_t wireless_console_start(char *out,size_t capacity)
{
 atomic_store(&desired,true);return wireless_start_action(out,capacity);
}
void wireless_console_ui(unsigned view)
{
 extern void serial_ui_view(unsigned);
 if(ctx.active&&ctx.state)serial_ui_view(view);
}
void wireless_console_report(void)
{
 extern void serial_ui_report(void);
 serial_transport_report();if(ctx.active&&ctx.state)serial_ui_report();
}
void wireless_console_send(const serial_frame_t *frame)
{
 extern void serial_ui_send_frame(const serial_frame_t*);
 if(ctx.active&&ctx.state)serial_ui_send_frame(frame);
}
