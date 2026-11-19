#include "voice_audio.h"
#include "board_audio.h"

/* Called exclusively by the XiaoZhi worker. Codec IO is 48 kHz mono PCM;
 * the existing protocol resampler produces 16 kHz Opus input. */
static esp_codec_dev_handle_t output, input, opened;
static voice_audio_mode_t mode;
static bool claimed, healthy;
bool voice_audio_init(void) { return true; }
bool voice_audio_ok(void) { return claimed && healthy; }
bool voice_audio_busy(void) { return claimed; }
size_t voice_audio_pending(void) { return 0; }
bool voice_audio_mode(voice_audio_mode_t next)
{
    if (next == mode && ((next == VOICE_AUDIO_OFF && !claimed) || (next != VOICE_AUDIO_OFF && healthy))) return true;
    if (opened) { esp_codec_dev_close(opened); opened = NULL; }
    mode = VOICE_AUDIO_OFF;
    if (next == VOICE_AUDIO_OFF) {
        if (claimed) board_audio_release();
        claimed = healthy = false; return true;
    }
    if (!claimed) {
        if (!board_audio_claim()) return false;
        claimed = true;
        if (board_audio_devices(&output, &input) != ESP_OK) {
            board_audio_release(); claimed = healthy = false; return false;
        }
    }
    healthy = true;
    if (next == VOICE_AUDIO_CAPTURE || next == VOICE_AUDIO_PLAY) {
        esp_codec_dev_sample_info_t format = {.sample_rate = 48000, .channel = 1, .bits_per_sample = 16};
        esp_codec_dev_handle_t device = next == VOICE_AUDIO_CAPTURE ? input : output;
        if (esp_codec_dev_open(device, &format) != ESP_CODEC_DEV_OK) { healthy = false; return false; }
        opened = device;
        int e = next == VOICE_AUDIO_CAPTURE ? esp_codec_dev_set_in_gain(device, 24) : esp_codec_dev_set_out_vol(device, 60);
        if (e != ESP_CODEC_DEV_OK) { esp_codec_dev_close(opened); opened = NULL; healthy = false; return false; }
    }
    if (next == VOICE_AUDIO_PLAY) board_audio_trace("xiaozhi/play");
    mode = next; return true;
}
size_t voice_audio_read(int16_t *samples, size_t capacity, unsigned timeout_ms)
{
    if (!timeout_ms || mode != VOICE_AUDIO_CAPTURE || !healthy || !capacity) return 0;
    if (capacity > 960) capacity = 960;
    if (esp_codec_dev_read(input, samples, capacity * sizeof(*samples)) != ESP_CODEC_DEV_OK) { healthy = false; return 0; }
    return capacity;
}
size_t voice_audio_write(const int16_t *samples, size_t count, unsigned timeout_ms)
{
    if (mode != VOICE_AUDIO_PLAY || !healthy || !count) return 0;
    if (count > 960) count = 960;
    if (esp_codec_dev_write(output, (void *)samples, count * sizeof(*samples)) != ESP_CODEC_DEV_OK) { healthy = false; return 0; }
    return count;
}

