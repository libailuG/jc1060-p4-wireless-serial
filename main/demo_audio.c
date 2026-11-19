#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdatomic.h>
#include "board_audio.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "demo.h"
#include "bsp/esp-bsp.h"
#include "esp_check.h"
#include "esp_heap_caps.h"

static esp_codec_dev_handle_t speaker, microphone;
static atomic_bool owned;
bool board_audio_claim(void) { bool expected = false; return atomic_compare_exchange_strong(&owned, &expected, true); }
void board_audio_release(void) { atomic_store(&owned, false); }
#define RATE 16000

static esp_err_t init_audio(void)
{
    ESP_RETURN_ON_ERROR(bsp_i2c_init(), "audio", "I2C");
    /* 先探测，避免未安装 codec 时进入厂商 BSP 的 assert。 */
    ESP_RETURN_ON_ERROR(i2c_master_probe(bsp_i2c_get_handle(), 0x18, 100), "audio", "ES8311 absent");
    if (!speaker) speaker = bsp_audio_codec_speaker_init();
    /* Single physical ES8311: share state instead of resetting a second codec. */
    microphone = speaker;
    return speaker && microphone ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t open_codec(esp_codec_dev_handle_t device)
{
    esp_codec_dev_sample_info_t format = {.sample_rate = RATE, .channel = 1, .bits_per_sample = 16};
    return esp_codec_dev_open(device, &format) == ESP_CODEC_DEV_OK ? ESP_OK : ESP_FAIL;
}
esp_err_t board_audio_devices(esp_codec_dev_handle_t *output, esp_codec_dev_handle_t *input)
{
    esp_err_t e = init_audio();
    if (e == ESP_OK) { *output = speaker; *input = microphone; }
    return e;
}

static esp_err_t play_pcm(const int16_t *pcm, size_t samples)
{
    ESP_RETURN_ON_ERROR(open_codec(speaker), "audio", "open output");
    esp_err_t ret = ESP_OK;
    if (esp_codec_dev_set_out_vol(speaker, 60) != ESP_CODEC_DEV_OK) ret = ESP_FAIL;
    board_audio_trace("tone/playback");
    if (ret == ESP_OK && esp_codec_dev_write(speaker, (void *)pcm, samples * sizeof(int16_t)) != ESP_CODEC_DEV_OK) ret = ESP_FAIL;
    esp_codec_dev_close(speaker);
    return ret;
}

static esp_err_t tone(char *result, size_t capacity)
{
    ESP_RETURN_ON_ERROR(init_audio(), "audio", "init");
    size_t count = RATE / 2;
    int16_t *pcm = heap_caps_malloc(count * sizeof(*pcm), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pcm) return ESP_ERR_NO_MEM;
    for (size_t i = 0; i < count; i++) pcm[i] = (int16_t)(5000.0f * sinf(2.0f * 3.14159265f * 440.0f * i / RATE));
    esp_err_t ret = play_pcm(pcm, count);
    free(pcm);
    snprintf(result, capacity, "440 Hz / 0.5 s / 16 kHz mono PCM. Verify sound from the speaker.");
    return ret;
}

static esp_err_t record_play(char *result, size_t capacity)
{
    ESP_RETURN_ON_ERROR(init_audio(), "audio", "init");
    const size_t bytes = RATE * 2 * sizeof(int16_t);
    int16_t *pcm = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!pcm) return ESP_ERR_NO_MEM;
    demo_post_status("Recording for 2 seconds. Speak into the microphone...");
    esp_err_t ret = open_codec(microphone);
    if (ret == ESP_OK) {
        if (esp_codec_dev_set_in_gain(microphone, 24) != ESP_CODEC_DEV_OK) ret = ESP_FAIL;
        if (ret == ESP_OK && esp_codec_dev_read(microphone, pcm, bytes) != ESP_CODEC_DEV_OK) ret = ESP_FAIL;
        esp_codec_dev_close(microphone);
    }
    int peak = 0;
    if (ret == ESP_OK) {
        for (size_t i = 0; i < bytes / sizeof(*pcm); ++i) {
            int value = abs((int)pcm[i]);
            if (value > peak) peak = value;
        }
        demo_post_status("Playing the 2-second recording...");
        ret = play_pcm(pcm, bytes / sizeof(*pcm));
    }
    free(pcm);
    snprintf(result, capacity, "Recorded in RAM, then played back. PCM peak=%d / 32768. No file saved.", peak);
    return ret;
}
esp_err_t demo_audio_tone(char *result, size_t capacity)
{
    if (!board_audio_claim()) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = tone(result, capacity); board_audio_release(); return ret;
}
esp_err_t demo_audio_record_play(char *result, size_t capacity)
{
    if (!board_audio_claim()) return ESP_ERR_INVALID_STATE;
    esp_err_t ret = record_play(result, capacity); board_audio_release(); return ret;
}

void board_audio_trace(const char *stage)
{
    int mute = -1, volume = -1, power = -1;
    esp_codec_dev_read_reg(speaker, 0x31, &mute);
    esp_codec_dev_read_reg(speaker, 0x32, &volume);
    esp_codec_dev_read_reg(speaker, 0x12, &power);
    gpio_input_enable(BSP_POWER_AMP_IO);
    ESP_LOGI("audio", "%s PA=%d DAC31=%02x volume32=%02x power12=%02x", stage,
        gpio_get_level(BSP_POWER_AMP_IO), mute, volume, power);
}
