#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include "platform.h"
#include "app_registry.h"
#include "demo_text.h"
#include "bsp/esp-bsp.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "weather.h"
#include "weather_images.h"

typedef struct { const char *title; demo_action_fn fn; } action_t;
static const action_t actions[] = {
    {"I2C 扫描", demo_i2c_scan}, {"RTC 读取", demo_rtc_show},
    {"TF 读写", demo_storage_test}, {"NVS 测试", demo_nvs_test},
    {"扬声器", demo_audio_tone}, {"录音回放", demo_audio_record_play},
    {"摄像头采集", demo_camera_capture}, {"Ethernet 启动", demo_ethernet_start},
    {"RS485 收发", demo_rs485_test}, {"USB 枚举", demo_usb_start},
    {"Wi-Fi 扫描", demo_wifi_scan},
};

static const app_descriptor_t *active_app;
static atomic_int requested_app=-1;
static lv_obj_t *shell_tabs[3];
static lv_obj_t *body, *heading, *status, *bar, *keyboard;
static lv_obj_t *ssid, *password, *name_input, *brightness, *idle_switch, *auto_switch, *boot_switch;
static lv_obj_t *info, *log_label;
static lv_obj_t *home_clock, *home_weather, *home_date;
static char last_status[512];
static bool dimmed;
static unsigned current_page;
static unsigned applied_brightness;
static lv_obj_t *ap_list;
static char scan_names[8][33];
static unsigned scan_count;
static int selected_ap = -1;
static void navigate(unsigned target);

static lv_obj_t *text(lv_obj_t *parent, int x, int y, int width, const char *content)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_color(label, lv_color_hex(0x15283f), 0);
    lv_obj_set_style_text_font(label, demo_text_font(), 0);
    lv_obj_set_pos(label, x, y); lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP); lv_label_set_text(label, content);
    return label;
}
static lv_obj_t *button(lv_obj_t *parent, int x, int y, int w, const char *title,
                        lv_event_cb_t callback, void *data)
{
    lv_obj_t *obj = lv_button_create(parent);
    lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, w, 48);
    lv_obj_add_event_cb(obj, callback, LV_EVENT_CLICKED, data);
    lv_obj_t *label = lv_label_create(obj);
    lv_obj_set_style_text_font(label, demo_text_font(), 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_label_set_text(label, title); lv_obj_center(label);
    return obj;
}
static void submit(const char *title, demo_action_fn fn)
{
    esp_err_t ret = platform_submit(title, fn);
    if (ret != ESP_OK) lv_label_set_text(status, "系统忙碌或服务不可用，请稍后重试。");
    else lv_label_set_text_fmt(status, "已提交：%s", title);
}
static void run(lv_event_t *event)
{
    const action_t *action = lv_event_get_user_data(event); submit(action->title, action->fn);
}
static void nav(lv_event_t *event) { navigate((unsigned)(uintptr_t)lv_event_get_user_data(event)); }
static void set_fullscreen(bool enabled)
{
    lv_obj_t *chrome[] = {heading, bar, shell_tabs[0], shell_tabs[1], shell_tabs[2]};
    for (unsigned i=0;i<5;i++) {
        if(enabled) lv_obj_add_flag(chrome[i],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(chrome[i],LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_pos(body,enabled?0:24,enabled?0:56);
    lv_obj_set_size(body,enabled?1024:976,enabled?600:482);
    lv_obj_scroll_to(body,0,0,LV_ANIM_OFF);
}
static void open_descriptor(const app_descriptor_t *app)
{
    if(active_app==app)return;
    navigate(4); active_app = app; lv_label_set_text(heading, app->name); set_fullscreen(app->fullscreen); app->open(body);
}
static void open_app(lv_event_t *event) { open_descriptor(lv_event_get_user_data(event)); }
void demo_request_app(const char *id)
{
    if(!strcmp(id,"home")){atomic_store(&requested_app,app_registry_count);return;}
    for(unsigned i=0;i<app_registry_count;i++)
        if(!strcmp(app_registry[i]->id,id)){atomic_store(&requested_app,i);return;}
}
static void keyboard_event(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_keyboard_set_textarea(keyboard, NULL); lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    }
}
static void focus_input(lv_event_t *event)
{
    lv_keyboard_set_textarea(keyboard, lv_event_get_target(event));
    lv_obj_remove_flag(keyboard, LV_OBJ_FLAG_HIDDEN); lv_obj_move_foreground(keyboard);
}
static lv_obj_t *input(int x, int y, int width, const char *initial, unsigned max_length, bool secret)
{
    lv_obj_t *obj = lv_textarea_create(body);
    lv_obj_set_pos(obj, x, y); lv_obj_set_size(obj, width, 48);
    lv_textarea_set_one_line(obj, true); lv_textarea_set_max_length(obj, max_length);
    lv_textarea_set_password_mode(obj, secret); lv_textarea_set_text(obj, initial);
    lv_obj_add_event_cb(obj, focus_input, LV_EVENT_CLICKED, NULL); return obj;
}
static lv_obj_t *toggle(int x, int y, bool on)
{
    lv_obj_t *obj = lv_switch_create(body); lv_obj_set_pos(obj, x, y);
    if (on) lv_obj_add_state(obj, LV_STATE_CHECKED);
    return obj;
}
static esp_err_t save_connect(char *out, size_t size)
{
    esp_err_t ret = platform_config_save(out, size);
    return ret == ESP_OK ? demo_wifi_connect(out, size) : ret;
}
static void ssid_edited(lv_event_t *event) { selected_ap = -1; }
static void ap_selected(lv_event_t *event)
{
    unsigned index = lv_dropdown_get_selected(ap_list);
    selected_ap = -1;
    if (!index || index > scan_count) return;
    char display[129];
    demo_text_copy(display, sizeof(display), scan_names[index - 1], 32, false);
    lv_textarea_set_text(ssid, display);
    selected_ap = (int)index - 1;
}
static void connect_clicked(lv_event_t *event)
{
    if (platform_busy()) { lv_label_set_text(status, "请等待当前任务完成。"); return; }
    const char *s = lv_textarea_get_text(ssid), *p = lv_textarea_get_text(password);
    if ((selected_ap < 0 && (!strlen(s) || strlen(s) > 32)) || strlen(p) > 63) {
        lv_label_set_text(status, "SSID 需要 1–32 字节，密码最多 63 字节。"); return;
    }
    platform_config_t cfg; platform_config_get(&cfg);
    if (selected_ap >= 0) memcpy(cfg.ssid, scan_names[selected_ap], sizeof(cfg.ssid));
    else snprintf(cfg.ssid, sizeof(cfg.ssid), "%s", s);
    snprintf(cfg.password, sizeof(cfg.password), "%s", p);
    cfg.auto_connect = lv_obj_has_state(auto_switch, LV_STATE_CHECKED);
    if (platform_config_set(&cfg) == ESP_OK) submit("保存并连接 Wi-Fi", save_connect);
}
static void save_settings(lv_event_t *event)
{
    if (platform_busy()) { lv_label_set_text(status, "请等待当前任务完成。"); return; }
    const char *name = lv_textarea_get_text(name_input);
    if (!strlen(name) || strlen(name) > 48) { lv_label_set_text(status, "设备名称需要 1–48 字节。"); return; }
    platform_config_t cfg; platform_config_get(&cfg);
    snprintf(cfg.device_name, sizeof(cfg.device_name), "%s", name);
    cfg.brightness = lv_slider_get_value(brightness);
    cfg.idle_seconds = lv_obj_has_state(idle_switch, LV_STATE_CHECKED) ? 60 : 0;
    cfg.boot_sample = lv_obj_has_state(boot_switch, LV_STATE_CHECKED);
    if (platform_config_set(&cfg) == ESP_OK) submit("保存设置", platform_config_save);
}
static void brightness_changed(lv_event_t *event) { bsp_display_brightness_set(lv_slider_get_value(brightness)); }
static void reset_confirm(lv_event_t *event)
{
    submit("恢复默认设置", platform_config_reset); lv_obj_delete(lv_event_get_user_data(event));
}
static void close_dialog(lv_event_t *event) { lv_obj_delete(lv_event_get_user_data(event)); }
static void reset_clicked(lv_event_t *event)
{
    lv_obj_t *dialog = lv_obj_create(lv_screen_active()); lv_obj_set_size(dialog, 560, 210); lv_obj_center(dialog);
    text(dialog, 12, 12, 500, "恢复系统默认设置？\n将清除保存的 Wi-Fi 凭据、设备名称及启动设置。\n正在使用的网络将在重启后按新配置运行。");
    button(dialog, 12, 126, 220, "取消", close_dialog, dialog);
    button(dialog, 270, 126, 220, "确认恢复", reset_confirm, dialog);
}
static void navigate(unsigned target)
{
    current_page = target;
    set_fullscreen(false);
    weather_desktop_activate(target == 0);
    if (active_app) { active_app->close(); active_app = NULL; }
    lv_keyboard_set_textarea(keyboard, NULL); lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(body); info = log_label = ap_list = status = home_clock = home_weather = home_date = NULL;
    selected_ap = -1; scan_count = 0; memset(scan_names, 0, sizeof(scan_names));
    platform_config_t cfg; platform_config_get(&cfg);
    if (!dimmed) bsp_display_brightness_set(cfg.brightness);
    const char *titles[] = {"应用首页", "网络", "系统设置", "诊断", "应用", "系统信息"};
    lv_label_set_text_fmt(heading, "TouchDesk / %s", titles[target]);
    last_status[0] = 0;
    if (target == 0) {
        lv_obj_t *hero = lv_obj_create(body);
        lv_obj_set_pos(hero, 0, 0); lv_obj_set_size(hero, 976, 130);
        lv_obj_set_style_pad_all(hero, 0, 0);
        lv_obj_set_style_bg_color(hero, lv_color_white(), 0);
        lv_obj_remove_flag(hero, LV_OBJ_FLAG_SCROLLABLE);
        home_clock = text(hero, 28, 12, 330, "--:--");
        lv_obj_set_style_text_font(home_clock, &lv_font_montserrat_48, 0);
        home_date = text(hero, 28, 84, 550, "连接网络后自动校时");
        home_weather = text(hero, 610, 35, 340, "天气尚未获取");
        const char *names[] = {"天气时钟", "有线串口", "无线串口", "键鼠桥接", "触屏键盘", "小智助手", "计数器", "音乐播放", "麦克风", "HC-08", "网络管理", "系统设置"};
        const char *icons[] = {LV_SYMBOL_HOME, LV_SYMBOL_USB, LV_SYMBOL_WIFI, LV_SYMBOL_BLUETOOTH, LV_SYMBOL_KEYBOARD, LV_SYMBOL_AUDIO, LV_SYMBOL_PLUS, LV_SYMBOL_AUDIO, LV_SYMBOL_VOLUME_MAX, LV_SYMBOL_BLUETOOTH, LV_SYMBOL_WIFI, LV_SYMBOL_SETTINGS};
        const app_descriptor_t *apps[] = {&weather_app, &serial_app, &wireless_app, &bridge_app, &keys_app, &xiaozhi_app, &counter_app, NULL, NULL, NULL, NULL, NULL};
        const uint32_t colors[] = {0xd9a023, 0x55718c, 0x087cff, 0x526fa8, 0x087cff, 0x7864da, 0x55718c, 0xe15383, 0x00a79c, 0x087cff, 0x00a79c, 0x55718c};
        for (unsigned i = 0; i < 12; i++) {
            lv_obj_t *tile = lv_button_create(body);
            lv_obj_set_pos(tile, (i % 6) * 164, 146 + (i / 6) * 160);
            lv_obj_set_size(tile, 156, 146);
            lv_obj_set_style_pad_all(tile, 0, 0);
            lv_obj_set_style_radius(tile, 14, 0);
            lv_obj_set_style_bg_color(tile, lv_color_white(), 0);
            lv_obj_set_style_border_width(tile, 1, 0);
            lv_obj_set_style_border_color(tile, lv_color_hex(0xe1e9f2), 0);
            lv_obj_set_style_shadow_width(tile, 0, 0);
            if (apps[i]) lv_obj_add_event_cb(tile, open_app, LV_EVENT_CLICKED, (void *)apps[i]);
            else if (i >= 10) lv_obj_add_event_cb(tile, nav, LV_EVENT_CLICKED, (void *)(uintptr_t)(i - 9));
            else lv_obj_add_state(tile, LV_STATE_DISABLED);
            if(i == 0) {
                lv_obj_t *weather_icon=lv_image_create(tile);
                lv_image_set_src(weather_icon,weather_image_source(2,true));
                lv_image_set_pivot(weather_icon,0,0);lv_image_set_scale(weather_icon,128);
                lv_obj_set_pos(weather_icon,46,14);
            }
            lv_obj_t *icon = text(tile, 0, 22, 156, i == 0 ? "" : icons[i]);
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_40, 0);
            lv_obj_set_style_text_align(icon, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(icon, lv_color_hex(colors[i]), 0);
            lv_obj_t *label = text(tile, 0, 86, 156, names[i]);
            lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
            if (i >= 7 && i <= 9) {
                lv_obj_t *hint = text(tile, 0, 114, 156, "待迁移");
                lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
                lv_obj_set_style_text_color(hint, lv_color_hex(0x8493a5), 0);
            }
        }
    } else if (target == 1) {
        text(body, 16, 8, 900, "选择附近网络，或手动输入网络名称与密码。");
        char display_ssid[129]; demo_text_copy(display_ssid, sizeof(display_ssid), cfg.ssid, 32, false);
        text(body, 16, 58, 100, "SSID"); ssid = input(110, 46, 330, display_ssid, 32, false);
        lv_obj_add_event_cb(ssid, ssid_edited, LV_EVENT_VALUE_CHANGED, NULL);
        text(body, 470, 58, 100, "密码"); password = input(550, 46, 380, cfg.password, 63, true);
        text(body, 16, 118, 250, "开机自动连接已保存网络"); auto_switch = toggle(275, 112, cfg.auto_connect);
        button(body, 16, 174, 220, "扫描附近网络", run, (void *)&actions[10]);
        button(body, 260, 174, 250, "保存并连接", connect_clicked, NULL);
        ap_list = lv_dropdown_create(body); lv_obj_set_pos(ap_list, 550, 174); lv_obj_set_size(ap_list, 380, 48);
        lv_dropdown_set_options(ap_list, "手动输入 / 扫描后选择网络");
        lv_obj_add_event_cb(ap_list, ap_selected, LV_EVENT_VALUE_CHANGED, NULL);
        text(body, 16, 250, 900, "可从下拉列表选择中文网络名；键盘支持英文、数字与符号。\n密码隐藏显示并保存到设备 NVS，当前固件未启用凭据加密。\n取得局域网 IP 不代表公网可用。C6 兼容性仍待连接测试。");
        status = text(body, 16, 330, 910, "");
    } else if (target == 2) {
        text(body, 16, 22, 110, "设备名称"); name_input = input(140, 8, 420, cfg.device_name, 48, false);
        text(body, 16, 90, 220, "背光 10–100%"); brightness = lv_slider_create(body);
        lv_obj_set_pos(brightness, 240, 94); lv_obj_set_size(brightness, 440, 18);
        lv_slider_set_range(brightness, 10, 100); lv_slider_set_value(brightness, cfg.brightness, LV_ANIM_OFF);
        lv_obj_add_event_cb(brightness, brightness_changed, LV_EVENT_VALUE_CHANGED, NULL);
        text(body, 16, 150, 320, "空闲 60 秒调暗，触摸恢复"); idle_switch = toggle(350, 145, cfg.idle_seconds != 0);
        text(body, 16, 210, 320, "开机直接进入示例应用"); boot_switch = toggle(350, 205, cfg.boot_sample);
        button(body, 16, 275, 220, "保存设置", save_settings, NULL);
        button(body, 270, 275, 250, "恢复默认设置…", reset_clicked, NULL);
        status = text(body, 16, 350, 910, "");
    } else if (target == 5) {
        info = text(body, 24, 24, 910, "");
        text(body, 24, 150, 910, "TouchDesk 基础系统\n音乐、麦克风、HC-08 待迁移。");
    } else if (target == 3) {
        info = text(body, 16, 8, 910, "");
        text(body, 16, 98, 910, "已实测：P4 启动、显示/触摸、C6 握手和 Wi-Fi 扫描。其余接口待验收。\n警告：Ethernet 启动后不可按 BOOT（GPIO35 与 TXD1 共用）。");
        for (unsigned i = 0; i < 10; i++)
            button(body, 16 + (i % 4) * 235, 170 + (i / 4) * 62, 218, actions[i].title, run, (void *)&actions[i]);
        status = text(body, 16, 370, 910, "");
        text(body, 16, 470, 910, "最近 8 条运行记录（重启清空，完整日志见串口）：");
        log_label = text(body, 16, 505, 910, "");
    }
}
static void refresh(lv_timer_t *timer)
{
    int requested=atomic_exchange(&requested_app,-1);
    if(requested==(int)app_registry_count)navigate(0);
    if(requested>=0&&(unsigned)requested<app_registry_count)open_descriptor(app_registry[requested]);
    if (home_clock) {
        time_t now = time(NULL); struct tm date; localtime_r(&now, &date);
        char line[160], lunar[64] = "";
        if (date.tm_year >= 124) {
            weather_lunar(date.tm_year + 1900, date.tm_mon + 1, date.tm_mday, lunar, sizeof(lunar));
            snprintf(line, sizeof(line), "%02d:%02d", date.tm_hour, date.tm_min);
            lv_label_set_text(home_clock, line);
            static const char *week[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
            lv_label_set_text_fmt(home_date, "%d月%d日  %s  %s", date.tm_mon + 1, date.tm_mday, week[date.tm_wday], lunar);
        } else {
            lv_label_set_text(home_clock, "--:--");
            lv_label_set_text(home_date, "连接网络后自动校时");
        }
        weather_snapshot_t w; weather_snapshot(&w);
        if (w.data.valid) lv_label_set_text_fmt(home_weather, "%s  %s  %.1f°C\n体感 %.1f°C  湿度 %.0f%%",
            weather_cities[w.city].name, weather_description(w.data.code),
            (double)w.data.temperature, (double)w.data.apparent, (double)w.data.humidity);
        else lv_label_set_text(home_weather, w.network ? "等待校时和天气更新" : "天气尚未获取，请连接 Wi-Fi");
    }
    if (ap_list) {
        char names[8][33]; unsigned count = demo_wifi_ap_snapshot(names);
        if (count != scan_count || memcmp(names, scan_names, sizeof(names))) {
            memcpy(scan_names, names, sizeof(names)); scan_count = count; selected_ap = -1;
            lv_dropdown_set_options(ap_list, "手动输入 / 选择网络");
            for (unsigned i = 0; i < count; i++) {
                char display[129]; demo_text_copy(display, sizeof(display), names[i], 32, false);
                lv_dropdown_add_option(ap_list, display, LV_DROPDOWN_POS_LAST);
            }
        }
    }
    char message[512]; platform_latest(message, sizeof(message));
    if (strcmp(message, last_status)) {
        snprintf(last_status, sizeof(last_status), "%s", message);
        if (status) {
            if (current_page != 3) message[strcspn(message, "\n")] = 0;
            lv_label_set_text(status, message);
        }
    }
    platform_config_t cfg; platform_config_get(&cfg);
    char network[32]; demo_wifi_status(network, sizeof(network));
    lv_label_set_text_fmt(bar, "%s | %s | Wi-Fi %s", cfg.device_name,
        platform_busy() ? "任务执行中" : "就绪", network);
    bool idle = cfg.idle_seconds && lv_display_get_inactive_time(NULL) > cfg.idle_seconds * 1000UL;
    if (idle != dimmed || applied_brightness != cfg.brightness) {
        dimmed = idle; applied_brightness = cfg.brightness;
        bsp_display_brightness_set(dimmed ? 10 : cfg.brightness);
    }
    if (info) lv_label_set_text_fmt(info,
        "系统 %s | ESP-IDF %s | 运行 %lld 秒 | 复位原因 %d\n内部空闲 %u KB | PSRAM 空闲 %u KB\n固件 %s | 编译 %s %s",
        PLATFORM_VERSION, esp_get_idf_version(), (long long)(esp_timer_get_time() / 1000000), esp_reset_reason(),
        (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
        (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024),
        esp_app_get_description()->version, __DATE__, __TIME__);
    if (log_label) { char history[1400]; platform_history(history, sizeof(history)); lv_label_set_text(log_label, history); }
}
void demo_ui_start(void)
{
    bsp_display_cfg_t display = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(), .buffer_size = BSP_LCD_DRAW_BUFF_SIZE,
        .double_buffer = false, .flags = {.buff_dma = false, .buff_spiram = true, .sw_rotate = true},
    };
    display.lvgl_port_cfg.task_stack = 16384; display.lvgl_port_cfg.task_priority = 4;
    assert(bsp_display_start_with_config(&display)); bsp_display_lock(0);
    ESP_ERROR_CHECK(demo_text_check());
    lv_obj_t *screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, demo_text_font(), 0);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xf2f6fc), 0);
    lv_obj_set_style_text_color(screen, lv_color_hex(0x15283f), 0);
    heading = text(screen, 24, 12, 330, ""); bar = text(screen, 370, 12, 630, "");
    const char *tabs[] = {"桌面", "系统信息", "诊断"};
    const unsigned pages[] = {0, 5, 3};
    for (unsigned i = 0; i < 3; i++) {
        lv_obj_t *tab = button(screen, 24 + i * 328, 548, 320, tabs[i], nav, (void *)(uintptr_t)pages[i]);
        shell_tabs[i]=tab;
        lv_obj_set_style_bg_color(tab, lv_color_hex(0xe4edf8), 0);
        lv_obj_set_style_text_color(lv_obj_get_child(tab, 0), lv_color_hex(0x15283f), 0);
    }
    body = lv_obj_create(screen); lv_obj_set_pos(body, 24, 56); lv_obj_set_size(body, 976, 482);
    lv_obj_set_style_pad_all(body, 0, 0); lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_text_color(body, lv_color_hex(0x15283f), 0);
    lv_obj_set_style_bg_color(body, lv_color_hex(0xf2f6fc), 0); lv_obj_set_scroll_dir(body, LV_DIR_VER);
    keyboard = lv_keyboard_create(screen); lv_obj_set_size(keyboard, 1024, 260);
    lv_obj_align(keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(keyboard, keyboard_event, LV_EVENT_ALL, NULL);
    platform_config_t cfg; platform_config_get(&cfg); navigate(cfg.boot_sample ? 4 : 0);
    if (cfg.boot_sample) { active_app = app_registry[0]; lv_label_set_text(heading, active_app->name); active_app->open(body); }
    lv_timer_create(refresh, 250, NULL); bsp_display_unlock();
    if (cfg.auto_connect && cfg.ssid[0]) platform_submit("开机连接 Wi-Fi", demo_wifi_connect);
}



