#include "app_serial.h"
#include "standalone.h"
#include "driver/uart.h"
#include "app.h"
#include "ui.h"
#define font_serial_status12 (*demo_text_font())
#include "weather_style.h"
#include "serial_transport.h"
#include "esp_timer.h"
#include "esp_log.h"

#include "serial_history.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include "value_display.h"
#include <stdlib.h>
LV_FONT_DECLARE(wireless_mono_40);
LV_FONT_DECLARE(wireless_ui_20);
LV_FONT_DECLARE(wireless_ui_28);
LV_FONT_DECLARE(wireless_ui_36);

#define STATE_COUNT 8u
typedef struct { app_ctx_t *ctx;unsigned index; } binding_t;
typedef struct {
 serial_frame_t tx;serial_uart_snapshot_t rx;
 lv_obj_t *fields[10],*values[10],*tabs[3],*heading,*hint,*send,*send_zero,*continuous_switch,*continuous_label,*result,*count;
 lv_obj_t *editor,*input,*validation,*matrix,*confirm;lv_font_t keypad_font;
 lv_obj_t *states[STATE_COUNT],*state_received[STATE_COUNT],*state_dots[STATE_COUNT];binding_t state_bindings[STATE_COUNT];
 binding_t bindings[10];bool receive,selftest,test_sent;int editing;int64_t entered;
 lv_timer_t *send_timer;
 bool plotting,ui_valid,last_receive,last_known;
 uint8_t state_keys[STATE_COUNT];
 char shown_values[10][7];
 serial_frame_t shown_frame;
 uint32_t ui_ticks,ui_max_us;

} state_t;
static atomic_bool test_requested,ui_requested,curve_requested,wireless_requested;
static bool report_pending;
static app_ctx_t *standalone_ctx;
extern const app_desc_t app_serial,app_wireless;
static void set_text_if_changed(lv_obj_t *obj,const char *text) {
 if(strcmp(lv_label_get_text(obj),text))lv_label_set_text(obj,text);
}
static void refresh(state_t *s);
void serial_app_selftest_request(void){atomic_store(&test_requested,true);atomic_store(&ui_requested,true);}
void wireless_app_selftest_request(void){atomic_store(&wireless_requested,true);atomic_store(&test_requested,true);atomic_store(&ui_requested,true);}
void serial_app_curve_request(void){atomic_store(&curve_requested,true);atomic_store(&ui_requested,true);}
void serial_app_poll(void) {
 if(atomic_exchange(&ui_requested,false))app_request(atomic_exchange(&wireless_requested,false)?&app_wireless:&app_serial);
 if(report_pending) {report_pending=false;}
}

static lv_obj_t *label(lv_obj_t *p,const char *t,int x,int y,int w,const lv_font_t *font) {
 lv_obj_t *l=ui_label(p,t,x,y,w);lv_obj_set_style_text_font(l,font,0);lv_label_set_long_mode(l,LV_LABEL_LONG_CLIP);lv_obj_clear_flag(l,LV_OBJ_FLAG_CLICKABLE);return l;
}
static const uint32_t field_colors[10]={0x2684ff,0x28bf42,0xffb21a,0xa65bff,0x08bcc8,0x2684ff,0x28bf42,0xffb21a,0xa65bff,0x08bcc8};
static lv_color_t card_color(unsigned index){(void)index;return lv_color_hex(0xf0f7ff);}
static lv_color_t card_ink(void){return lv_color_hex(0x173a5b);}
static void close_editor(state_t *s){if(s->editor){lv_obj_del(s->editor);s->editor=NULL;s->input=NULL;}s->editing=-1;}
static bool commit(state_t *s) {
 if(!serial_value_parse(&s->tx,(unsigned)s->editing,lv_textarea_get_text(s->input))){set_text_if_changed(s->validation,s->editing<2?"整数范围 0~4294967295":"请输入有效的浮点数");return false;}
 close_editor(s);refresh(s);return true;
}
static void keypad(lv_event_t *e) {
 if(lv_event_get_code(e)!=LV_EVENT_VALUE_CHANGED)return;
 state_t *s=((app_ctx_t*)lv_event_get_user_data(e))->state;
 uint16_t id=lv_btnmatrix_get_selected_btn(s->matrix);if(id==LV_BTNMATRIX_BTN_NONE)return;
 const char *key=lv_btnmatrix_get_btn_text(s->matrix,id);
 if(!strcmp(key,"确定")){commit(s);return;}
 if(!strcmp(key,"取消")){close_editor(s);return;}
 if(!strcmp(key,"清空")){lv_textarea_set_text(s->input,"");return;}
 if(!strcmp(key,"退格")||!strcmp(key,"<")){lv_textarea_delete_char(s->input);return;}
 if(!strcmp(key,"+/-")) {
  const char *text=lv_textarea_get_text(s->input);char next[40];
  if(*text=='-')snprintf(next,sizeof(next),"%s",text+1);else snprintf(next,sizeof(next),"-%s",text);
  lv_textarea_set_text(s->input,next);lv_textarea_set_cursor_pos(s->input,LV_TEXTAREA_CURSOR_LAST);return;
 }
 lv_textarea_add_text(s->input,key);
}
static void editor_confirm(lv_event_t *e) {
 if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
 app_ctx_t *ctx=lv_event_get_user_data(e);state_t *s=ctx->state;
 if(ctx->active&&s->editor)commit(s);
}
static const char *keymap[]={"7","8","9","退格","\n","4","5","6","清空","\n","1","2","3","e","\n","+/-","0",".","取消",""};
static void edit(lv_event_t *e) {
 if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
 binding_t *b=lv_event_get_user_data(e);app_ctx_t *ctx=b->ctx;state_t *s=ctx->state;
 if(!ctx->active||s->receive||s->plotting||s->editor)return;
 s->editing=(int)b->index;
 s->editor=lv_obj_create(ctx->root);lv_obj_remove_style_all(s->editor);lv_obj_set_size(s->editor,1024,600);lv_obj_set_style_bg_color(s->editor,ui_background(),0);lv_obj_set_style_bg_opa(s->editor,LV_OPA_COVER,0);lv_obj_clear_flag(s->editor,LV_OBJ_FLAG_SCROLLABLE);
 char title[32];snprintf(title,sizeof(title),"编辑 data%u",b->index+1);label(s->editor,title,24,12,400,&font_ui);
 label(s->editor,b->index<2?"uint32":"float32",850,20,150,&lv_font_montserrat_14);
 s->input=lv_textarea_create(s->editor);lv_obj_set_pos(s->input,24,60);lv_obj_set_size(s->input,750,64);lv_textarea_set_one_line(s->input,true);lv_textarea_set_max_length(s->input,32);
 lv_obj_set_style_text_font(s->input,&lv_font_montserrat_26,0);
 lv_obj_set_style_pad_hor(s->input,10,0);lv_obj_set_style_pad_ver(s->input,6,0);lv_obj_set_style_border_width(s->input,1,0);lv_obj_set_style_radius(s->input,8,0);lv_obj_set_style_bg_color(s->input,ui_surface(),0);lv_obj_set_style_text_color(s->input,ui_foreground(),0);
 lv_textarea_set_accepted_chars(s->input,b->index<2?"0123456789":"0123456789.+-eE");
 char value[40];serial_value_format(&s->tx,b->index,value,sizeof(value),true);lv_textarea_set_text(s->input,value);lv_textarea_set_cursor_pos(s->input,LV_TEXTAREA_CURSOR_LAST);
 s->validation=label(s->editor,b->index<2?"0~4294967295":"小数 / 负数 / e",420,12,400,&font_ui);
 s->matrix=lv_btnmatrix_create(s->editor);lv_obj_set_pos(s->matrix,24,140);lv_obj_set_size(s->matrix,976,436);lv_btnmatrix_set_map(s->matrix,keymap);
 s->confirm=ui_button(s->editor,"确定",800,60,200,64,editor_confirm,ctx);
 lv_obj_set_style_bg_color(s->confirm,lv_color_hex(weather_palette(ui_theme())->accent),0);lv_obj_set_style_text_color(s->confirm,ui_accent_foreground(),0);
 s->keypad_font=lv_font_montserrat_32;s->keypad_font.fallback=&font_ui;
 lv_obj_set_style_pad_all(s->matrix,0,0);lv_obj_set_style_pad_row(s->matrix,6,0);lv_obj_set_style_pad_column(s->matrix,8,0);
 lv_obj_set_style_radius(s->matrix,9,LV_PART_ITEMS);lv_obj_set_style_border_width(s->matrix,0,LV_PART_ITEMS);lv_obj_set_style_shadow_width(s->matrix,0,LV_PART_ITEMS);
 lv_obj_set_style_text_font(s->matrix,&s->keypad_font,LV_PART_ITEMS);lv_obj_set_style_bg_color(s->matrix,ui_surface(),LV_PART_ITEMS);lv_obj_set_style_text_color(s->matrix,ui_foreground(),LV_PART_ITEMS);lv_obj_set_style_bg_color(s->matrix,ui_background(),0);lv_obj_set_style_border_width(s->matrix,0,0);
 lv_btnmatrix_set_btn_ctrl_all(s->matrix,LV_BTNMATRIX_CTRL_NO_REPEAT);
 if(b->index<2){lv_btnmatrix_set_btn_ctrl(s->matrix,11,LV_BTNMATRIX_CTRL_DISABLED);lv_btnmatrix_set_btn_ctrl(s->matrix,12,LV_BTNMATRIX_CTRL_DISABLED);lv_btnmatrix_set_btn_ctrl(s->matrix,14,LV_BTNMATRIX_CTRL_DISABLED);}
 lv_obj_add_event_cb(s->matrix,keypad,LV_EVENT_VALUE_CHANGED,ctx);
}
static void transmit(state_t *s) {
 set_text_if_changed(s->result,serial_transport_send(&s->tx)?(serial_transport_active()?"已排队":"发送成功"):"发送失败");refresh(s);
}
static void send_once(lv_event_t *e) {
 if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
 app_ctx_t *ctx=lv_event_get_user_data(e);state_t *s=ctx->state;
 if(!ctx->active||s->receive||s->plotting||s->editor)return;
 transmit(s);
}
static void send_zero_once(lv_event_t *e) {
 if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
 app_ctx_t *ctx=lv_event_get_user_data(e);state_t *s=ctx->state;
 if(!ctx->active||s->receive||s->plotting||s->editor)return;
 /* Reset the editable model too, so the panel and subsequent sends stay zero. */
 serial_frame_init(&s->tx);
 bool accepted=serial_transport_send(&s->tx);
 refresh(s);
 set_text_if_changed(s->result,accepted?(serial_transport_active()?"已排队":"发送成功"):"发送失败");
}
static void continuous_tick(lv_timer_t *timer) {
 app_ctx_t *ctx=lv_timer_get_user_data(timer);state_t *s=ctx->state;
 if(!ctx->active||s->editor||!lv_obj_has_state(s->continuous_switch,LV_STATE_CHECKED))return;
 /* One attempt per tick; transport backpressure never causes catch-up bursts. */
 if(!serial_transport_send(&s->tx))set_text_if_changed(s->result,"发送失败");
}
static void continuous_changed(lv_event_t *e) {
 if(lv_event_get_code(e)!=LV_EVENT_VALUE_CHANGED)return;
 app_ctx_t *ctx=lv_event_get_user_data(e);state_t *s=ctx->state;
 if(lv_obj_has_state(s->continuous_switch,LV_STATE_CHECKED)&&ctx->active){
  lv_timer_reset(s->send_timer);lv_timer_resume(s->send_timer);
 }else{
  lv_obj_clear_state(s->continuous_switch,LV_STATE_CHECKED);lv_timer_pause(s->send_timer);
 }
}
static void toggle_state(lv_event_t *e) {
 if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
 binding_t *b=lv_event_get_user_data(e);state_t *s=b->ctx->state;
 if(!b->ctx->active||s->receive||s->plotting||s->editor)return;
 s->tx.data1^=UINT32_C(1)<<b->index;transmit(s);
}
static void refresh(state_t *s) {
 serial_transport_snapshot(&s->rx);
 serial_frame_t empty;serial_frame_init(&empty);
 const serial_frame_t *frame=s->receive?(s->rx.has_frame?&s->rx.last:&empty):&s->tx;
 bool force=!s->ui_valid||s->last_receive!=s->receive||s->last_known!=s->rx.has_frame;
 /* Update styles only when a visible state changes; avoid invalidating cards at 10Hz. */
 for(unsigned i=0;i<STATE_COUNT;i++) {
  bool known=s->rx.has_frame;
  bool requested=(s->tx.data1>>i)&1u,received=known&&((s->rx.last.data1>>i)&1u);
  uint8_t key=(requested?1:0)|(received?2:0);
  if(!force&&s->state_keys[i]==key)continue;
  s->state_keys[i]=key;
  char text[64];lv_color_t bg=lv_color_hex(0xe0edfc),ink=ui_foreground();
  if(s->receive){
   snprintf(text,sizeof(text),"%s",known?(received?"开":"关"):"?");
   if(received){bg=lv_color_hex(0xe0f8e4);ink=ui_foreground();}
   lv_obj_add_flag(s->state_dots[i],LV_OBJ_FLAG_HIDDEN);
   lv_obj_set_style_border_width(s->states[i],1,0);
   lv_obj_clear_flag(s->states[i],LV_OBJ_FLAG_CLICKABLE);
  }else{
   snprintf(text,sizeof(text),"%s",requested?"开":"关");
   if(requested){bg=lv_color_hex(0xe0f8e4);ink=ui_foreground();}
   lv_obj_clear_flag(s->state_dots[i],LV_OBJ_FLAG_HIDDEN);
   /* Always opaque: an unknown RX must never reveal the requested-state background. */
   lv_obj_set_style_bg_color(s->state_dots[i],known?lv_color_hex(received?0x43d47a:0x6b7280):ui_surface(),0);
   lv_obj_set_style_bg_opa(s->state_dots[i],LV_OPA_COVER,0);
   lv_obj_set_style_border_color(s->state_dots[i],lv_color_hex(0xffffff),0);
   lv_obj_set_style_border_width(s->states[i],1,0);
   lv_obj_add_flag(s->states[i],LV_OBJ_FLAG_CLICKABLE);
  }
  set_text_if_changed(s->state_received[i],text);

  bool active=s->receive?received:requested;
  lv_obj_set_style_border_color(s->states[i],lv_color_hex(active?0x52bf65:0x8eb3df),0);
  lv_obj_set_style_bg_grad_color(s->states[i],lv_color_hex(active?0xcff1d7:0xd0e4fc),0);
  lv_obj_set_style_bg_grad_dir(s->states[i],LV_GRAD_DIR_VER,0);
  lv_obj_set_style_bg_color(s->states[i],bg,0);lv_obj_set_style_text_color(s->states[i],ink,0);
  lv_obj_set_style_bg_color(s->states[i],bg,LV_STATE_PRESSED);lv_obj_set_style_text_color(s->states[i],ink,LV_STATE_PRESSED);
 }
 bool values_changed=force||memcmp(&s->shown_frame,frame,sizeof(*frame));
 for(unsigned i=0;i<10;i++) {
  if(!values_changed)break;
  double value;
  if(i<2)value=i?frame->data2:frame->data1;
  else {float v;memcpy(&v,(const uint8_t*)frame+12+4*(i-2),4);value=v;}
  char compact[7];wireless_number_display(value,compact);
  const char *shown=s->receive&&!s->rx.has_frame?"--":compact;
  if(strcmp(s->shown_values[i],shown)){
   strcpy(s->shown_values[i],shown);
   set_text_if_changed(s->values[i],shown);
  }
  if(force){
   if(s->receive)lv_obj_clear_flag(s->fields[i],LV_OBJ_FLAG_CLICKABLE);
   else lv_obj_add_flag(s->fields[i],LV_OBJ_FLAG_CLICKABLE);
  }
 }
 s->shown_frame=*frame;
 s->ui_valid=true;s->last_receive=s->receive;s->last_known=s->rx.has_frame;
 char count[48];snprintf(count,sizeof(count),"TX %lu   RX %lu",(unsigned long)s->rx.tx,(unsigned long)s->rx.rx);set_text_if_changed(s->count,count);
 if(serial_transport_active())set_text_if_changed(s->result,serial_transport_status());
 else if(s->receive)set_text_if_changed(s->result,s->rx.has_frame?"已更新接收数据":"等待接收");
}
static void tab(lv_event_t *e) {
 if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
 app_ctx_t *ctx=lv_event_get_user_data(e);state_t *s=ctx->state;
 s->plotting=false;s->receive=lv_event_get_target(e)==s->tabs[1];
 set_text_if_changed(s->heading,s->receive?"接收数据":"发送数据");set_text_if_changed(s->hint,"状态1~8");

 if(s->receive||s->plotting){
  lv_obj_clear_state(s->continuous_switch,LV_STATE_CHECKED);lv_timer_pause(s->send_timer);
  lv_obj_add_flag(s->send,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(s->send_zero,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(s->continuous_switch,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(s->continuous_label,LV_OBJ_FLAG_HIDDEN);
 }else{
  lv_obj_clear_flag(s->send,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(s->send_zero,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(s->continuous_switch,LV_OBJ_FLAG_HIDDEN);lv_obj_clear_flag(s->continuous_label,LV_OBJ_FLAG_HIDDEN);
 }
 for(unsigned i=0;i<2;i++){
  bool active=i==(s->plotting?2u:(unsigned)s->receive);
  lv_obj_set_style_bg_color(s->tabs[i],active?lv_color_hex(0x087bff):ui_surface(),0);
  lv_obj_set_style_text_color(s->tabs[i],active?lv_color_white():ui_foreground(),0);
 }
 set_text_if_changed(s->result,s->receive?"等待接收":"待发送");refresh(s);
}

static void tick(lv_timer_t *t) {
 app_ctx_t *ctx=lv_timer_get_user_data(t);
 if(!ctx->active||!ctx->state)return;
 state_t *s=ctx->state;
 int64_t begin=esp_timer_get_time();
 refresh(s);
 uint32_t elapsed=(uint32_t)(esp_timer_get_time()-begin);
 if(elapsed>s->ui_max_us)s->ui_max_us=elapsed;
 s->ui_ticks++;
}
static void leave(lv_event_t *e) {
 if(lv_event_get_code(e)!=LV_EVENT_CLICKED)return;
 app_ctx_t *ctx=lv_event_get_user_data(e);state_t *s=ctx->state;
 if(s->editor){close_editor(s);return;}
 app_request(NULL);
}
static esp_err_t create(app_ctx_t *ctx,lv_obj_t *root) {
 standalone_ctx=ctx;state_t *s=ctx->state;
 lv_obj_set_size(root,1024,600);
 serial_frame_init(&s->tx);s->editing=-1;s->entered=esp_timer_get_time();s->selftest=atomic_exchange(&test_requested,false);
 if(s->selftest) {
  const char *values[]={"305419896","4294967295","-1.25","0","1.5","42","0.0001","1000","-2.5","3.25"};
  for(unsigned i=0;i<10;i++)serial_value_parse(&s->tx,i,values[i]);
 }
 lv_obj_set_style_bg_color(root,lv_color_hex(0xf0f7ff),0);lv_obj_set_style_bg_grad_color(root,lv_color_hex(0xe0edfc),0);lv_obj_set_style_bg_grad_dir(root,LV_GRAD_DIR_VER,0);lv_obj_set_style_bg_opa(root,LV_OPA_COVER,0);
 ui_button(root,"返回桌面",20,12,140,48,leave,ctx);
 label(root,"ESP-NOW 无线串口",180,18,312,&wireless_ui_28);
 s->tabs[0]=ui_button(root,"发送",600,12,194,48,tab,ctx);s->tabs[1]=ui_button(root,"接收",810,12,194,48,tab,ctx);
 lv_obj_set_style_bg_color(s->tabs[0],lv_color_hex(0x087bff),0);lv_obj_set_style_text_color(s->tabs[0],lv_color_white(),0);
 s->heading=label(root,"发送数据",12,0,140,&font_ui);s->hint=label(root,"状态1~8",166,0,146,&font_ui);lv_obj_add_flag(s->heading,LV_OBJ_FLAG_HIDDEN);lv_obj_add_flag(s->hint,LV_OBJ_FLAG_HIDDEN);
 for(unsigned i=0;i<STATE_COUNT;i++) {
  s->state_bindings[i]=(binding_t){ctx,i};
  s->states[i]=ui_button(root,"",20+124*i,80,116,100,toggle_state,&s->state_bindings[i]);
  lv_obj_clean(s->states[i]);lv_obj_set_style_pad_all(s->states[i],0,0);
  lv_obj_set_style_clip_corner(s->states[i],true,0);
  lv_obj_set_style_radius(s->states[i],10,0);lv_obj_set_style_shadow_width(s->states[i],0,0);
  lv_obj_set_style_border_width(s->states[i],1,0);
  lv_obj_set_style_border_color(s->states[i],lv_color_hex(field_colors[i]),0);
  lv_obj_set_style_border_post(s->states[i],true,0);
  char caption[24];snprintf(caption,sizeof(caption),"状态%u",i+1);
  label(s->states[i],caption,8,8,80,&wireless_ui_20);
  s->state_received[i]=label(s->states[i],"?",0,0,108,&wireless_ui_36);
  lv_obj_set_style_text_align(s->state_received[i],LV_TEXT_ALIGN_CENTER,0);lv_obj_align(s->state_received[i],LV_ALIGN_BOTTOM_MID,0,-10);
  s->state_dots[i]=lv_obj_create(s->states[i]);lv_obj_remove_style_all(s->state_dots[i]);
  lv_obj_set_pos(s->state_dots[i],92,8);lv_obj_set_size(s->state_dots[i],16,16);
  lv_obj_set_style_radius(s->state_dots[i],LV_RADIUS_CIRCLE,0);
  lv_obj_set_style_border_width(s->state_dots[i],3,0);
  lv_obj_clear_flag(s->state_dots[i],LV_OBJ_FLAG_CLICKABLE|LV_OBJ_FLAG_SCROLLABLE);

 }
 for(unsigned i=0;i<10;i++) {
  /* Native landscape grid; field identity is explicit on every card. */
  const struct {int x,y,w,h;} card={20+(i%5)*200,210+(i/5)*130,184,110};
  s->fields[i]=ui_button(root,"",card.x,card.y,card.w,card.h,edit,&s->bindings[i]);lv_obj_clean(s->fields[i]);
  s->bindings[i]=(binding_t){ctx,i};lv_obj_set_style_pad_all(s->fields[i],0,0);
  lv_obj_set_style_border_width(s->fields[i],1,0);lv_obj_set_style_border_color(s->fields[i],lv_color_hex(field_colors[i]),0);
  lv_obj_set_style_radius(s->fields[i],10,0);lv_obj_set_style_shadow_width(s->fields[i],0,0);
  lv_obj_set_style_bg_grad_color(s->fields[i],lv_color_hex(0xe2efff),0);lv_obj_set_style_bg_grad_dir(s->fields[i],LV_GRAD_DIR_VER,0);
  lv_obj_set_style_bg_color(s->fields[i],card_color(i),0);lv_obj_set_style_text_color(s->fields[i],card_ink(),0);
  lv_obj_set_style_bg_color(s->fields[i],lv_color_mix(card_color(i),card_ink(),210),LV_STATE_PRESSED);
  lv_obj_set_style_text_color(s->fields[i],card_ink(),LV_STATE_PRESSED);
  char caption[32];snprintf(caption,sizeof(caption),"data%u  %s",i+1,i<2?"uint32":"float32");
  label(s->fields[i],caption,10,10,170,&lv_font_montserrat_18);
  s->values[i]=label(s->fields[i],"0",8,30,168,&wireless_mono_40);
  lv_obj_set_style_text_align(s->values[i],LV_TEXT_ALIGN_RIGHT,0);lv_obj_align(s->values[i],LV_ALIGN_BOTTOM_RIGHT,-8,-16);
 }
 s->send=ui_button(root,"发送一次",20,478,260,56,send_once,ctx);
 lv_obj_set_style_text_font(s->send,&wireless_ui_28,0);
 s->send_zero=ui_button(root,"全零发送",300,478,260,56,send_zero_once,ctx);
 lv_obj_set_style_text_font(s->send_zero,&wireless_ui_28,0);
 lv_obj_set_style_border_color(s->send_zero,lv_color_hex(0x087bff),0);
 lv_obj_set_style_text_color(s->send_zero,lv_color_hex(0x087bff),0);
 s->continuous_label=label(root,"持续发送20ms",710,493,230,&wireless_ui_20);
 s->continuous_switch=lv_switch_create(root);lv_obj_set_pos(s->continuous_switch,938,490);lv_obj_set_size(s->continuous_switch,66,36);
 lv_obj_set_style_bg_color(s->continuous_switch,lv_color_hex(0xa8b4c4),LV_PART_MAIN);
 lv_obj_set_style_bg_color(s->continuous_switch,lv_color_hex(0x087bff),LV_PART_INDICATOR|LV_STATE_CHECKED);
 lv_obj_set_style_bg_color(s->continuous_switch,lv_color_white(),LV_PART_KNOB);
 lv_obj_add_event_cb(s->continuous_switch,continuous_changed,LV_EVENT_VALUE_CHANGED,ctx);
 s->send_timer=app_timer_create(ctx,continuous_tick,20);if(!s->send_timer)return ESP_ERR_NO_MEM;lv_timer_pause(s->send_timer);
 lv_obj_set_style_bg_color(s->send,lv_color_hex(weather_palette(ui_theme())->accent),0);lv_obj_set_style_text_color(s->send,ui_accent_foreground(),0);
 lv_obj_t *divider=lv_obj_create(root);lv_obj_remove_style_all(divider);lv_obj_set_pos(divider,0,548);lv_obj_set_size(divider,1024,1);lv_obj_set_style_bg_color(divider,lv_color_hex(0x8eb3df),0);lv_obj_set_style_bg_opa(divider,LV_OPA_COVER,0);
 s->result=label(root,"待发送",20,558,650,&wireless_ui_20);s->count=label(root,"TX 0   RX 0",700,558,300,&lv_font_montserrat_20);
 refresh(s);atomic_store(&curve_requested,false);return app_timer_create(ctx,tick,100)?ESP_OK:ESP_ERR_NO_MEM;
}
static void destroy(app_ctx_t *ctx) {
 state_t *s=ctx->state;if(!s)return;
 if(standalone_ctx==ctx)standalone_ctx=NULL;

 close_editor(s);
}
static bool back(app_ctx_t *ctx){state_t *s=ctx->state;if(s->editor){close_editor(s);return true;}return false;}
const app_desc_t app_serial={.id="serial",.title="串口助手",.subtitle="结构体收发",.caps=APP_CAP_FULLSCREEN|APP_CAP_SERIAL,.state_size=sizeof(state_t),.create=create,.destroy=destroy,.back=back};

const app_desc_t app_wireless={.id="wireless",.title="无线串口",.subtitle="ESP-NOW",.caps=APP_CAP_FULLSCREEN|APP_CAP_SERIAL|APP_CAP_WIRELESS,.state_size=sizeof(state_t),.create=create,.destroy=destroy,.back=back};

void serial_ui_send_frame(const serial_frame_t *f){
 if(!standalone_ctx)return;
 state_t *s=standalone_ctx->state;s->tx=*f;
 lv_obj_send_event(s->tabs[0],LV_EVENT_CLICKED,NULL);
 lv_obj_send_event(s->send,LV_EVENT_CLICKED,NULL);
}
void serial_ui_view(unsigned index){
 if(standalone_ctx&&index<2){state_t *s=standalone_ctx->state;lv_obj_send_event(s->tabs[index],LV_EVENT_CLICKED,NULL);}
}
void serial_ui_report(void){
 if(!standalone_ctx)return;
 state_t *s=standalone_ctx->state;char b[512];
 int n=snprintf(b,sizeof(b),"{\"ui\":true,\"view\":\"%s\",\"curve_enabled\":false,\"history_points\":0,\"ui_ticks\":%lu,\"ui_max_us\":%lu,\"free_internal\":%u,\"largest_internal\":%u,\"free_psram\":%u}\r\n",s->receive?"receive":"send",(unsigned long)s->ui_ticks,(unsigned long)s->ui_max_us,(unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL),(unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
 if(n>0&&n<(int)sizeof(b))printf("%.*s",n,b);
}
