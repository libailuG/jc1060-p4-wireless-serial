#include <stdint.h>
#include <string.h>
#include "demo_text.h"
#include "esp_log.h"

LV_FONT_DECLARE(board_font_cjk_16);
static lv_font_t interface_font;

const lv_font_t *demo_text_font(void)
{
    /* 不修改 LVGL 的 const 全局字体；保留英文字形并串联中文后备字体。 */
    if (!interface_font.get_glyph_dsc) {
        interface_font = lv_font_montserrat_16;
        interface_font.fallback = &board_font_cjk_16;
        interface_font.line_height = board_font_cjk_16.line_height;
        interface_font.base_line = board_font_cjk_16.base_line;
    }
    return &interface_font;
}

static size_t sequence_length(const uint8_t *p, size_t left)
{
    if (p[0] < 0x80) return 1;
    size_t n = p[0] >= 0xc2 && p[0] <= 0xdf ? 2 :
               p[0] >= 0xe0 && p[0] <= 0xef ? 3 :
               p[0] >= 0xf0 && p[0] <= 0xf4 ? 4 : 0;
    if (!n || left < n) return 0;
    for (size_t i = 1; i < n; ++i) if ((p[i] & 0xc0) != 0x80) return 0;
    if ((p[0] == 0xe0 && p[1] < 0xa0) || (p[0] == 0xed && p[1] >= 0xa0) ||
        (p[0] == 0xf0 && p[1] < 0x90) || (p[0] == 0xf4 && p[1] >= 0x90)) return 0;
    return n;
}

size_t demo_text_copy(char *dst, size_t capacity, const void *src, size_t length, bool multiline)
{
    if (!dst || !capacity) return 0;
    dst[0] = 0;
    if (!src) return 0;
    const uint8_t *p = src;
    static const char hex[] = "0123456789ABCDEF";
    size_t written = 0;
    for (size_t i = 0; i < length && p[i];) {
        size_t count = sequence_length(p + i, length - i);
        bool control = p[i] < 0x20 || p[i] == 0x7f;
        if (multiline && (p[i] == '\n' || p[i] == '\t')) control = false;
        if (!count || control) {
            if (capacity - written <= 4) break;
            dst[written++] = '\\'; dst[written++] = 'x';
            dst[written++] = hex[p[i] >> 4]; dst[written++] = hex[p[i] & 15];
            i++;
        } else {
            if (capacity - written <= count) break;
            memcpy(dst + written, p + i, count);
            written += count;
            i += count;
        }
    }
    dst[written] = 0;
    return written;
}

esp_err_t demo_text_check(void)
{
    const lv_font_t *font = demo_text_font();
    unsigned checked = 0;
    for (uint32_t cp = 0x20; cp <= 0x7e; ++cp) {
        lv_font_glyph_dsc_t glyph;
        if (!lv_font_get_glyph_dsc(font, &glyph, cp, 0) || glyph.is_placeholder) {
            ESP_LOGE("font", "Missing ASCII U+%04lx", (unsigned long)cp);
            return ESP_FAIL;
        }
        checked++;
    }
    const uint32_t sample[] = {0x4e2d,0x6587,0x65e0,0x7ebf,0x7f51,0x7edc,0x529e,0x516c,0x5ba4,
        0x5ba2,0x5385,0x8363,0x8000,0x534e,0x4e3a,0x5c0f,0x7c73,0x8def,0x7531,0x5668,0xe9,0x2019,0xff08,0xff09};
    for (size_t i = 0; i < sizeof(sample) / sizeof(sample[0]); i++) {
        lv_font_glyph_dsc_t glyph;
        if (!lv_font_get_glyph_dsc(font, &glyph, sample[i], 0) || glyph.is_placeholder) {
            ESP_LOGE("font", "Missing sample U+%04lx", (unsigned long)sample[i]);
            return ESP_FAIL;
        }
        checked++;
    }
    char output[32], boundary[6];
    const uint8_t bad_ssid[] = {'A', 0xff, 'B', '\n', 0};
    demo_text_copy(output, sizeof(output), bad_ssid, sizeof(bad_ssid), false);
    demo_text_copy(boundary, sizeof(boundary), "中文", strlen("中文"), true);
    if (strcmp(output, "A\\xFFB\\x0A") || strcmp(boundary, "中")) return ESP_FAIL;
    ESP_LOGI("font", "PASS: %u ASCII/CJK/Latin glyphs; UTF-8 boundary and invalid-byte checks", checked);
    return ESP_OK;
}
