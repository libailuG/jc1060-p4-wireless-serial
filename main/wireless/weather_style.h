#pragma once
#include "lvgl.h"
#define WEATHER_THEMES 3
#define WEATHER_DEFAULT_THEME 1
typedef struct {uint32_t bg,ink,sub,line,surface,accent;} weather_palette_t;
const weather_palette_t *weather_palette(unsigned theme);
