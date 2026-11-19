#pragma once
#include <stddef.h>
#include <stdbool.h>
#include "lvgl.h"
#include "esp_err.h"

/* UTF-8 完整字符边界复制；无效字节显示为 ASCII 的 \xNN。返回输出字节数。 */
size_t demo_text_copy(char *dst, size_t capacity, const void *src, size_t length, bool multiline);
const lv_font_t *demo_text_font(void);
esp_err_t demo_text_check(void);
