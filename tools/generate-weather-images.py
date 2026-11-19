"""Convert legacy weather PNG assets to LVGL 9 ARGB8888 (requires Pillow)."""
from pathlib import Path
import argparse
from PIL import Image
parser=argparse.ArgumentParser()
parser.add_argument('--size',type=int,default=128)
args=parser.parse_args()
root=Path(__file__).resolve().parents[1]
names=['sun','moon','partly-day','partly-night','cloud','fog','drizzle','rain','heavy-rain','thunder','snow','unknown','moderate-rain','showers-day','freezing-rain','showers-night']
p=root/'main/weather/weather_images.c'
existing=p.read_text(encoding='utf-8')
mapping=existing[existing.index('const lv_image_dsc_t *weather_image_source'):]
out=['/* Generated from assets/weather-soft3d; run tools/generate-weather-images.py. */','#include "weather_images.h"']
for i,name in enumerate(names):
    im=Image.open(root/'assets/weather-soft3d'/f'{name}-196.png').convert('RGBA').resize((args.size,args.size),Image.Resampling.LANCZOS)
    raw=im.tobytes();data=bytearray(len(raw))
    data[0::4]=raw[2::4];data[1::4]=raw[1::4];data[2::4]=raw[0::4];data[3::4]=raw[3::4]
    out.append(f'static const uint8_t pixels_{i}[]={{'+','.join(map(str,data))+'};')
    out.append(f'static const lv_image_dsc_t image_{i}={{.header={{.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_ARGB8888,.w={args.size},.h={args.size},.stride={args.size*4}}},.data_size=sizeof(pixels_{i}),.data=pixels_{i}}};')
p.write_text('\n'.join(out)+'\n'+mapping,encoding='utf-8')
