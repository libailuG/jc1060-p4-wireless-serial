#include <time.h>
#include "app_registry.h"
#include "demo_text.h"

static lv_timer_t *ticker;
static lv_obj_t *clock_label, *date_label;

static void tick(lv_timer_t *timer)
{
    time_t now = time(NULL);
    struct tm date;
    gmtime_r(&now, &date);
    if (date.tm_year < 124) {
        lv_label_set_text(clock_label, "--:--:--");
        lv_label_set_text(date_label, "尚未校时");
        return;
    }
    /* Display UTC+8 without changing the process timezone or RTC. */
    now += 8 * 3600;
    gmtime_r(&now, &date);
    char text[64];
    strftime(text, sizeof(text), "%H:%M:%S", &date);
    lv_label_set_text(clock_label, text);
    strftime(text, sizeof(text), "%Y-%m-%d  UTC+8", &date);
    lv_label_set_text(date_label, text);
}

static void open_clock(lv_obj_t *parent)
{
    clock_label = lv_label_create(parent);
    lv_obj_set_style_text_font(clock_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_transform_scale_x(clock_label, 512, 0);
    lv_obj_set_style_transform_scale_y(clock_label, 512, 0);
    lv_obj_align(clock_label, LV_ALIGN_TOP_MID, -60, 50);
    date_label = lv_label_create(parent);
    lv_obj_align(date_label, LV_ALIGN_TOP_MID, 0, 125);
    lv_obj_t *hint = lv_label_create(parent);
    lv_obj_set_style_text_font(hint, demo_text_font(), 0);
    lv_label_set_text(hint, "北京时间；等待后续网络校时服务接入。");
    lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 200);
    tick(NULL);
    ticker = lv_timer_create(tick, 250, NULL);
}

static void close_clock(void)
{
    if (ticker) lv_timer_delete(ticker);
    ticker = NULL;
    clock_label = date_label = NULL;
}

const app_descriptor_t clock_app = {
    .id = "clock", .name = "时钟", .description = "北京时间与日期",
    .open = open_clock, .close = close_clock,
};
