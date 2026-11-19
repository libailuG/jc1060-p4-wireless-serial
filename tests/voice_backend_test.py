"""Run the real board voice adapter against a fake codec (no hardware access)."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CC = os.environ.get('HOST_CC', 'C:/Users/libai/Documents/Codex/2026-10-19/esp32s3-app-platform/work/host-tools/tcc/tcc.exe')
with tempfile.TemporaryDirectory(prefix='p4-voice-test-') as temp:
    p = Path(temp)
    (p / 'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n')
    (p / 'esp_codec_dev.h').write_text('''#pragma once
typedef void *esp_codec_dev_handle_t;
typedef struct {int sample_rate,channel,bits_per_sample;} esp_codec_dev_sample_info_t;
#define ESP_CODEC_DEV_OK 0
int esp_codec_dev_close(void*);
int esp_codec_dev_open(void*,esp_codec_dev_sample_info_t*);
int esp_codec_dev_set_in_gain(void*,float);
int esp_codec_dev_set_out_vol(void*,int);
int esp_codec_dev_read(void*,void*,int);
int esp_codec_dev_write(void*,void*,int);
''')
    (p / 'test.c').write_text('''#include <assert.h>
#include <stdio.h>
#include "board_audio.h"
#include "voice_audio.h"
static int owned, fail_open, closes, writes, reads;
bool board_audio_claim(void){if(owned)return false;owned=1;return true;}
void board_audio_release(void){assert(owned);owned=0;}
void board_audio_trace(const char *stage){}
esp_err_t board_audio_devices(void **out,void **in){*out=(void*)1;*in=(void*)2;return 0;}
int esp_codec_dev_close(void *d){closes++;return 0;}
int esp_codec_dev_open(void *d,esp_codec_dev_sample_info_t *f){assert(f->sample_rate==48000&&f->channel==1&&f->bits_per_sample==16);return fail_open;}
int esp_codec_dev_set_in_gain(void *d,float g){assert(d==(void*)2);return 0;}
int esp_codec_dev_set_out_vol(void *d,int g){assert(d==(void*)1);return 0;}
int esp_codec_dev_read(void *d,void *p,int n){assert(d==(void*)2&&n<=1920);reads++;return 0;}
int esp_codec_dev_write(void *d,void *p,int n){assert(d==(void*)1&&n<=1920);writes++;return 0;}
int main(void){
 int16_t pcm[1000];
 assert(voice_audio_init());
 assert(voice_audio_mode(VOICE_AUDIO_IDLE)&&owned);
 fail_open=1;assert(!voice_audio_mode(VOICE_AUDIO_CAPTURE));
 assert(voice_audio_mode(VOICE_AUDIO_OFF)&&!owned);
 fail_open=0;assert(voice_audio_mode(VOICE_AUDIO_CAPTURE));
 assert(voice_audio_read(pcm,1000,20)==960&&reads==1);
 assert(voice_audio_mode(VOICE_AUDIO_PLAY)&&closes==1);
 assert(voice_audio_write(pcm,1000,20)==960&&writes==1);
 assert(voice_audio_read(pcm,1000,20)==0);
 assert(voice_audio_mode(VOICE_AUDIO_OFF)&&!owned&&closes==2);
 assert(voice_audio_mode(VOICE_AUDIO_OFF)&&!owned);
 owned=1;assert(!voice_audio_mode(VOICE_AUDIO_IDLE));owned=0;
 puts("PASS voice mode switching, codec format, chunk bounds, ownership and failed-open recovery");
 return 0;
}
''')
    exe = p / 'test.exe'
    subprocess.run([CC, '-I'+str(p), '-I'+str(ROOT/'main'), '-I'+str(ROOT/'main/services'),
                    str(p/'test.c'), str(ROOT/'main/services/voice_audio.c'), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)

