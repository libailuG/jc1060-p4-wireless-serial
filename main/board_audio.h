#pragma once
#include "esp_codec_dev.h"
#include "esp_err.h"
#include <stdbool.h>
bool board_audio_claim(void);
void board_audio_release(void);
esp_err_t board_audio_devices(esp_codec_dev_handle_t *output, esp_codec_dev_handle_t *input);
void board_audio_trace(const char *stage);
