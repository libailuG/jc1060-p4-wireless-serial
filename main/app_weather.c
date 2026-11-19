#include <stdio.h>
#include "app_registry.h"
#include "weather.h"
#include "weather_images.h"
#include "demo_text.h"
#include "platform.h"

static lv_timer_t *timer;
static lv_obj_t *root, *clock_label, *date_label, *city, *panel, *stamp;
static bool seconds, hourly;
static unsigned revision;
static int minute_key;
static lv_obj_t *label(lv_obj_t *p,int x,int y,int w,const char *s) {
    lv_obj_t *o=lv_label_create(p); lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w);
    lv_obj_set_style_text_font(o,demo_text_font(),0);
    lv_obj_set_style_text_color(o,lv_color_hex(0x263e36),0);
    lv_label_set_text(o,s); return o;
}
static void image(lv_obj_t *p,int x,int y,int size,int code,bool day) {
    lv_obj_t *o=lv_image_create(p); lv_image_set_src(o,weather_image_source(code,day));
    lv_image_set_pivot(o,0,0); lv_image_set_scale(o,size*256/128); lv_obj_set_pos(o,x,y);
}
static lv_obj_t *button(lv_obj_t *p,int x,int y,int w,const char *s,lv_event_cb_t cb) {
    lv_obj_t *o=lv_button_create(p);lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,42);
    lv_obj_set_style_bg_color(o,lv_color_hex(0xe7ece5),0);lv_obj_set_style_shadow_width(o,0,0);
    lv_obj_t *l=label(o,0,0,w-12,s);lv_obj_center(l);lv_obj_set_style_text_align(l,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_add_event_cb(o,cb,LV_EVENT_CLICKED,NULL);return o;
}
static void update(lv_timer_t *unused);
static void toggle_seconds(lv_event_t *e) { seconds=!seconds;update(NULL); }
static void toggle_hourly(lv_event_t *e) { hourly=!hourly;revision=~0u;update(NULL); }
static void refresh_weather(lv_event_t *e) { if(!weather_refresh()) lv_label_set_text(stamp,"暂时无法刷新，请稍后重试"); }
static void choose(lv_event_t *e) {
    if(weather_choose_city(lv_dropdown_get_selected(city))!=ESP_OK) {
        weather_snapshot_t s;weather_snapshot(&s);lv_dropdown_set_selected(city,s.city);
        lv_label_set_text(stamp,"城市保存失败，请重试");
    }
    revision=~0u;update(NULL);
}
static void draw_weather(const weather_snapshot_t *s,time_t now) {
    lv_obj_clean(panel);
    const weather_data_t *d=&s->data;
    bool valid=d->valid&&d->city==s->city&&now-d->data_time<6*3600;
    char buf[160];struct tm today;localtime_r(&now,&today);
    if(!hourly) {
        image(panel,480,25,176,valid?d->code:-1,d->daytime);
        snprintf(buf,sizeof(buf),valid?"%.0f°":"--°",(double)d->temperature);
        lv_obj_t *temp=label(panel,690,55,240,buf);lv_obj_set_style_text_font(temp,&lv_font_montserrat_48,0);
        label(panel,695,118,220,valid?weather_description(d->code):"暂无有效天气");
        const weather_day_t *day=NULL;
        for(int i=0;i<WEATHER_DAYS;i++)if(valid&&d->days[i].date==weather_date_key(&today))day=&d->days[i];
        if(day)snprintf(buf,sizeof(buf),"%.0f° — %.0f°",(double)day->low,(double)day->high);else snprintf(buf,sizeof(buf),"--° / --°");
        label(panel,695,151,230,buf);
        if(valid)snprintf(buf,sizeof(buf),"体感 %.0f°C    湿度 %.0f%%    风 %.1f m/s",(double)d->apparent,(double)d->humidity,(double)d->wind);
        else snprintf(buf,sizeof(buf),"%s",s->network?"等待有效天气数据":"请连接 Wi-Fi 获取天气");
        label(panel,480,207,475,buf);
    } else {
        unsigned next=0;while(next<d->hour_count&&d->hours[next].time<=now)next++;
        for(unsigned i=0;i<5;i++) {
            const weather_hour_t *h=valid&&s->time_valid&&next+i<d->hour_count?&d->hours[next+i]:NULL;
            int x=480+i*94;
            if(h){struct tm t;localtime_r(&h->time,&t);strftime(buf,sizeof(buf),"%H:%M",&t);}else snprintf(buf,sizeof(buf),"--:--");
            label(panel,x,38,90,buf);image(panel,x,68,72,h?h->code:-1,h?h->daytime:true);
            if(h)snprintf(buf,sizeof(buf),"%.0f°  %d%%",(double)h->temperature,h->precipitation);else snprintf(buf,sizeof(buf),"--°  --%%");
            label(panel,x,158,92,buf);
        }
        label(panel,480,207,460,"每小时温度 / 降水概率");
    }
    button(panel,728,242,226,hourly?"返回当前天气":"查看未来5小时",toggle_hourly);
    label(panel,24,300,200,"未来三天");
    const char *week[]={"周日","周一","周二","周三","周四","周五","周六"};
    for(int i=0;i<3;i++) {
        struct tm future=today;future.tm_mday+=i+1;mktime(&future);
        const weather_day_t *day=NULL;
        for(int j=0;j<WEATHER_DAYS;j++)if(valid&&d->days[j].date==weather_date_key(&future))day=&d->days[j];
        int x=30+i*320;
        if(s->time_valid)snprintf(buf,sizeof(buf),"%s %d日",i?week[future.tm_wday]:"明天",future.tm_mday);else snprintf(buf,sizeof(buf),"等待校时");
        label(panel,x,334,160,buf);image(panel,x+140,325,70,day?day->code:-1,true);
        if(day)snprintf(buf,sizeof(buf),"%.0f° / %.0f°",(double)day->low,(double)day->high);else snprintf(buf,sizeof(buf),"--° / --°");
        label(panel,x,375,145,buf);
    }
}
static void update(lv_timer_t *unused) {
    if(!root)return;
    weather_snapshot_t s;weather_snapshot(&s);time_t now=weather_now();struct tm t;localtime_r(&now,&t);
    char buf[160];
    if(s.time_valid) {
        strftime(buf,sizeof(buf),seconds?"%H:%M:%S":"%H:%M",&t);lv_label_set_text(clock_label,buf);
        char lunar[64]="";weather_lunar(t.tm_year+1900,t.tm_mon+1,t.tm_mday,lunar,sizeof(lunar));
        const char *week[]={"星期日","星期一","星期二","星期三","星期四","星期五","星期六"};
        snprintf(buf,sizeof(buf),"%d月%d日  %s\n%s",t.tm_mon+1,t.tm_mday,week[t.tm_wday],lunar);
    }else {lv_label_set_text(clock_label,seconds?"--:--:--":"--:--");snprintf(buf,sizeof(buf),"等待网络校时");}
    lv_label_set_text(date_label,buf);
    lv_obj_set_style_transform_scale_x(clock_label,seconds?300:440,0);
    lv_obj_set_style_transform_scale_y(clock_label,440,0);
    int key=(int)(now/60);
    if(revision!=s.revision||minute_key!=key) {
        revision=s.revision;minute_key=key;draw_weather(&s,now);
        const char *state=!s.network?"离线":!s.time_valid?"等待校时":s.phase==WEATHER_FETCHING?"正在更新":s.phase==WEATHER_ERROR?"更新失败":s.phase==WEATHER_LIMITED?"服务限流":!s.data.valid?"等待天气":now-s.data.data_time>=3600?"天气已过期":"已更新";
        if(s.data.valid){struct tm dt;localtime_r(&s.data.data_time,&dt);snprintf(buf,sizeof(buf),"%s %02d:%02d",state,dt.tm_hour,dt.tm_min);}else snprintf(buf,sizeof(buf),"%s",state);
        lv_label_set_text(stamp,buf);
    }
}
static void close_settings(lv_event_t *e) { lv_obj_delete(lv_event_get_user_data(e)); }
static void seconds_changed(lv_event_t *e) {
    seconds=lv_obj_has_state(lv_event_get_target(e),LV_STATE_CHECKED);update(NULL);
}
static void settings(lv_event_t *e) {
    lv_obj_t *dialog=lv_obj_create(root);lv_obj_set_size(dialog,440,220);lv_obj_center(dialog);
    lv_obj_set_style_bg_color(dialog,lv_color_hex(0xf5f3ec),0);
    label(dialog,12,8,360,"天气时钟 · 显示设置");
    label(dialog,12,56,250,"显示秒数（本次会话）");
    lv_obj_t *sw=lv_switch_create(dialog);lv_obj_set_pos(sw,300,50);
    if(seconds)lv_obj_add_state(sw,LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw,seconds_changed,LV_EVENT_VALUE_CHANGED,NULL);
    label(dialog,12,100,380,"纸白墨绿主题 · 温度 °C · 风速 m/s");
    lv_obj_t *done=lv_button_create(dialog);lv_obj_set_pos(done,260,145);lv_obj_set_size(done,130,42);
    lv_obj_t *txt=label(done,0,0,100,"完成");lv_obj_center(txt);
    lv_obj_add_event_cb(done,close_settings,LV_EVENT_CLICKED,dialog);
}
static void open_weather(lv_obj_t *parent) {
    weather_activate(true);seconds=false;hourly=false;revision=~0u;minute_key=-1;
    root=lv_obj_create(parent);lv_obj_remove_style_all(root);lv_obj_set_size(root,976,482);
    lv_obj_set_style_bg_color(root,lv_color_hex(0xf5f3ec),0);lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
    lv_obj_remove_flag(root,LV_OBJ_FLAG_SCROLLABLE);
    panel=lv_obj_create(root);lv_obj_remove_style_all(panel);lv_obj_set_size(panel,976,425);lv_obj_remove_flag(panel,LV_OBJ_FLAG_SCROLLABLE);
    clock_label=label(root,28,64,260,"--:--");lv_obj_set_style_text_font(clock_label,&lv_font_montserrat_48,0);
    lv_obj_set_style_transform_pivot_x(clock_label,0,0);lv_obj_set_style_transform_pivot_y(clock_label,0,0);
    lv_obj_add_flag(clock_label,LV_OBJ_FLAG_CLICKABLE);lv_obj_add_event_cb(clock_label,toggle_seconds,LV_EVENT_CLICKED,NULL);
    date_label=label(root,28,185,420,"");lv_obj_set_style_text_align(date_label,LV_TEXT_ALIGN_CENTER,0);
    label(root,110,250,300,"点击时钟显示 / 隐藏秒数");
    city=lv_dropdown_create(root);lv_obj_set_pos(city,480,0);lv_obj_set_size(city,170,42);
    lv_dropdown_set_options(city,"杭州\n上海\n北京\n深圳\n成都");
    weather_snapshot_t s;weather_snapshot(&s);lv_dropdown_set_selected(city,s.city);
    lv_obj_add_event_cb(city,choose,LV_EVENT_VALUE_CHANGED,NULL);
    stamp=label(root,30,447,600,"");button(root,650,432,140,"刷新天气",refresh_weather);button(root,810,432,140,"设置",settings);
    update(NULL);timer=lv_timer_create(update,1000,NULL);
}
static void close_weather(void) {
    if(timer) lv_timer_delete(timer);
    timer=NULL;root=NULL;
    weather_activate(false);
}
const app_descriptor_t weather_app={.id="weather",.name="天气时钟",.description="时钟、当前天气与预报",.open=open_weather,.close=close_weather};
