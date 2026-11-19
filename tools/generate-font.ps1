param(
    [string]$Converter = "$PSScriptRoot\..\..\..\tmp\font-tools\node_modules\lv_font_conv\lv_font_conv.js",
    [string]$Python = 'C:\Espressif\tools\python_env\idf6.1_py3.11_env\Scripts\python.exe'
)
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$font = Join-Path $project 'managed_components\lvgl__lvgl\scripts\built_in_font\SourceHanSansSC-Normal.otf'
if (!(Test-Path -LiteralPath $font)) { throw 'Source font not found.' }
if (!(Test-Path -LiteralPath $Converter)) { throw 'Install lv_font_conv@1.5.3 and pass -Converter.' }
& $Python (Join-Path $PSScriptRoot 'collect-font-characters.py')
if ($LASTEXITCODE -ne 0) { throw 'Character collection failed.' }
$characters = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'font-characters.txt'))
$output = Join-Path $project 'main\fonts\board_font_cjk_16.c'
& node $Converter --font $font --range '0xa0-0x24f,0x2000-0x206f,0x20a0-0x214f,0x2190-0x21ff,0x3000-0x303f,0xff00-0xffef' --symbols $characters --size 16 --bpp 4 --no-compress --no-kerning --format lvgl --lv-font-name board_font_cjk_16 -o $output
if ($LASTEXITCODE -ne 0) { throw 'Font conversion failed.' }
$code = [IO.File]::ReadAllText($output).Replace('#include "lvgl/lvgl.h"', '#include "lvgl.h"')
[IO.File]::WriteAllText($output, $code, [Text.UTF8Encoding]::new($false))
