#pragma once
#include <stdbool.h>
#include "esp_err.h"
esp_err_t c6_stage_command(const char *line);
bool c6_stage_receiving(void);
esp_err_t c6_stage_data(const char *line);
void c6_stage_feed(const void *bytes,size_t size);
esp_err_t c6_update(char *out,size_t capacity);
