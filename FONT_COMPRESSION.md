# 中文字体无损压缩记录

## 已回退

用户实机反馈后要求取消压缩，已恢复压缩前完整字体及原生成脚本（默认 `--no-compress`）。
重新构建的镜像与压缩前备份 hash 完全一致，已写回 COM13，烧录 hash 校验通过并复位。
以下是已结束的压缩试验记录，命令和数据只供历史参考，不代表当前源码/固件。
验证脚本已移至工作区 `tmp/font-compression/verify-font-compression.py` 留档。

日期：2026-10-30。本次只更换字体数据及生成脚本，不改字形范围、字号或灰度。
压缩镜像位于 build/jc1060_hw_starter.bin，随后已按用户要求烧录到 COM13，写入 hash 校验通过。

## 实测体积

以下 MB/KB 使用十进制，比较相同工程的未压缩和压缩字体版本。

| 项目 | 压缩前（字节） | 压缩后（字节） | 节省（字节） |
|---|---:|---:|---:|
| 字体链接数据 | 3,840,777 | 3,319,204 | 521,573（13.58%） |
| 整个固件 bin | 5,447,152 | 4,925,584 | 521,568（9.58%） |
| 8 MiB 应用分区剩余比例 | 35.07% | 41.28% | 增加约 6.22 个百分点 |

字库仍含 28,631 个字形，保留 117 个 cmap、原 Unicode 映射、16px 字号、4bpp 灰度、
字形宽高/偏移/advance、行高和基线。未裁剪罕见汉字，也未改成 1bpp/2bpp。

比较了 lv_font_conv 1.5.3 的两种无损模式：

| 模式 | 位图数据（字节，不含 glyph 描述符与 cmap） |
|---|---:|
| 原始未压缩 | 3,379,494 |
| RLE + XOR 预处理（采用） | 2,857,921 |
| RLE 无预处理 | 2,867,687 |

字形描述符和 Unicode 索引仍需保留；灰度边缘也包含较多不同像素，因此不能仅靠无损压缩把整个字库变成几十 KB。
下一步若要显著缩小，需要另行评估字形裁剪或灰度降低，它们不在本次修改范围内。

## 验证

- `tools/verify-font-compression.py` 按 LVGL 9.5 的 RLE 状态机和 XOR 反滤波规则解码生成的 C 数组。
- 两种候选均与原字库逐字比较：全部 **28,631 字形、6,756,376 像素完全一致**。
  Unicode 映射、排版度量、行高、基线均相同。
- `build.ps1 build` 成功，修改源码无新增编译警告或错误。
- 应用大小 `0x4b2890`，在原 8 MiB 应用分区内，无需改分区。
- esptool image-info：ESP32-P4、16 MB Flash、revision 3.1–3.99，checksum/hash 均有效。
- 镜像 SHA256：`F9CADBEAFD4A73CBCF4F1CFD2B20FA83B011D540DB6D3805DE665BD8D0B5186E`。

此主机测试不等于实机渲染测试。压缩版运行时需解压字形，额外 CPU 开销及界面刷新速度尚未实测。
已烧录后通过 USB `scan` 做运行检查，C6 握手成功、扫描返回 ESP_OK 和 8 个热点，包含中文名称。
观察期间未见 panic 或反复重启；中文系统页面、滚动、弹出键盘和中文热点的实际观感待用户确认。
当前不宣称速度与原版完全相同。烧录/运行日志为工作区 `tmp/font-compression/flash.log`、`runtime.log`。

## 重建与回退

```powershell
# 默认无损压缩
.\tools\generate-font.ps1
.\build.ps1 build

# 回到原始不压缩模式（字形范围/字号/灰度相同）
.\tools\generate-font.ps1 -Uncompressed
.\build.ps1 build
```

生成脚本仍使用 lv_font_conv 1.5.3，字体源仍为 LVGL 随附 SourceHanSansSC-Normal.otf。
正常编译无需转换工具，继续保留原 OFL 字体许可证。

工作区临时文件：

- `tmp/font-compression/board_font_cjk_16.uncompressed.c`：修改前字体。
- `tmp/font-compression/jc1060-uncompressed.bin`：修改前底座镜像。
- `tmp/font-compression/generate-font.original.ps1`：修改前生成脚本。
- `tmp/font-compression/no-prefilter.c`：未采用的无预处理候选。
- `tmp/font-compression/build.log`、`size-files.csv`：构建与链接体积记录。

从工程目录重跑全量对比：

```text
python tools/verify-font-compression.py ../../tmp/font-compression/board_font_cjk_16.uncompressed.c main/fonts/board_font_cjk_16.c
```
