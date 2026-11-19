#pragma once
#include "lvgl.h"
#include "demo_text.h"
#define font_ui (*demo_text_font())
bool ui_dark_theme(void);
unsigned ui_theme(void);
lv_color_t ui_background(void);
lv_color_t ui_foreground(void);
lv_color_t ui_surface(void);
lv_color_t ui_accent_foreground(void);
lv_obj_t *ui_button(lv_obj_t*,const char*,int,int,int,int,lv_event_cb_t,void*);
lv_obj_t *ui_label(lv_obj_t*,const char*,int,int,int);
