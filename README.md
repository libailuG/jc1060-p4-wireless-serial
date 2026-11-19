# JC1060P470 V3 触屏应用底座

## 当前上传版本（2026-11-19）

目标硬件：JC1060P470 V3，ESP32-P4 revision 3.2，1024×600 触摸屏；开发环境为 ESP-IDF 6.1。

无线串口当前使用 **ESP-NOW**，包含发送、接收两页：

- 一行 8 个状态按钮，对应 `data1` 的 bit0～bit7。
- 10 项数据采用等宽字体、右对齐，最多显示 6 个字符，超出隐藏，不采用科学计数法；编辑及协议值保留原精度。
- “发送一次”和“全零发送”；全零发送会清零发送面板并提交一帧有效协议数据。
- 暂时关闭曲线入口、图表缓存和后台采样；相同内容不重复更新界面样式。

协议为 48 字节小端帧：`0xAAAAAAAA + 2×uint32 + 8×float32 + 0xEEEEEEEE`。
这份仓库是 **P4 应用工程**；ESP-NOW 对端及板载 C6 固件需与其匹配。独立的外接 C6 BLE 通用透传工程不在本仓库内，不能直接与此 ESP-NOW 页面互通。

在已初始化的 ESP-IDF 6.1 终端中运行：

```powershell
idf.py build
idf.py -p COM13 flash
```

COM13 为此前本机 P4 端口，使用前需核实实际端口。保留 `sdkconfig`、`sdkconfig.defaults` 和 `dependencies.lock`；首次构建需要下载组件。
本仓库不包含 `build/`、`managed_components/`、设备 Flash 读回、NVS 或运行日志。上传前配置已检查，后续填写网络密码或令牌后不要提交明文凭据。

最新实机记录见 [VALIDATION.md](VALIDATION.md)，曲线屏蔽和卡顿排查见 [WIRELESS_PERFORMANCE.md](WIRELESS_PERFORMANCE.md)。两分钟无流量驻留检查通过，不代表已经完成数小时持续收发验收。

第三方组件保留各自的许可证；本仓库未新增覆盖所有组件的统一许可证。

---

当前迁移版本 **0.6.0-dev**，新增 ESP-NOW 无线串口，接线、两端固件和验证入口见 [ESP_NOW.md](ESP_NOW.md)，完整迁移记录见 [MIGRATION.md](MIGRATION.md)。

下方为原应用底座 **0.1.0** 的说明。新增四页系统外壳、应用生命周期、后台服务、
NVS 配置、屏幕 Wi-Fi 配网与运行记录。开发入口及本版验收见 [PLATFORM.md](PLATFORM.md)。
原硬件说明保留在下方；历史固件的实机结果不代表新界面已经验收。
此前的 `dev/jc1060_hw_starter_source.zip` 是旧基础示例，未自动覆盖。

适用于本次连接的 **JC1060P470C_I_W_Y V3、ESP32-P4 revision 3.2 + ESP32-C6**，
1024×600 触摸屏、16 MB Flash、32 MB PSRAM。基于厂商 IDF 6.1 示例的板级驱动整理。
目标是让你能找到每个外设的初始化、读写和错误处理入口，然后逐步替换为自己的业务。

工程路径：`C:\play\esp32p4-esp32c6\dev\jc1060_hw_starter`。
它自带 BSP、OV02C10 和 IP101 源码，不依赖旁边的 `common_components`。
首次构建需联网下载 `idf_component.yml` 中的依赖；保留 `dependencies.lock` 固定实际版本。

## 1. 先运行起来

本机已安装 ESP-IDF 6.1，使用 PowerShell：

```powershell
cd C:\play\esp32p4-esp32c6\dev\jc1060_hw_starter
.\build.ps1 build
.\build.ps1 flash COM13
.\build.ps1 monitor COM13
```

串口监视退出：`Ctrl+]`。先退出串口监视，再进行下一次烧录。
如果 PowerShell 限制运行脚本，可在已初始化 ESP-IDF 6.1 的终端直接执行：

```text
idf.py build
idf.py -p COM13 flash
idf.py -p COM13 monitor
```

保留随工程提供的 `sdkconfig`。它包含 P4 v3.1+、PSRAM、LCD、摄像头、C6 SDIO 配置。
不要照着其他 P4 开发板重新选择引脚。若需要重建配置，以保留的 `sdkconfig.defaults` 为基础。

烧录本版后屏幕提供首页、网络、设置、诊断四个入口；外设测试集中在诊断页。
界面使用中文；已加入 16px 思源黑体后备字库，支持中文 Wi-Fi 名称和中英文混排。
英文主字体为 Montserrat 16px，说明见下方“字体与 Wi-Fi 名称”。
启动只初始化屏幕、触摸、BOOT 按钮及网络基础设施，其余外设在点击时才初始化。

原出厂固件备份：
`C:\play\esp32p4-esp32c6\backups\2026-10-30\p4-com13-before-lvgl9.bin`。
新工程使用与上次 LVGL 示例相同的分区布局；常规 flash 只写 bootloader、分区表和应用。
不要使用 erase-flash 来“解决”普通外设问题。

## 2. 每项资源对应哪段代码

| 资源 | 文件 / 入口 | 示例行为 |
|---|---|---|
| 屏幕、触摸 | `main/demo_ui.c` / `demo_ui_start()` | 创建界面、按钮事件、显示触摸坐标 |
| 背光 PWM | `brightness_changed()` | 调用 `bsp_display_brightness_set(10..100)` |
| I2C 总线 | `main/demo_io.c` / `demo_i2c_scan()` | 扫描 7 位地址，复用 BSP 的总线句柄 |
| RTC RX8025T | `demo_rtc_read()` / `demo_rtc_write()` | BCD 时间读写、掉电失效标志检查；按钮只读 |
| BOOT 按键 | `demo_button_start()` | GPIO35 轮询、约 30 ms 消抖、统计按下次数 |
| TF / SDMMC | `main/demo_storage.c` / `demo_storage_test()` | 挂载、创建临时文件、写入、回读比对、删除临时文件 |
| NVS / Flash | `demo_nvs_test()` | 独立 namespace 保存计数器，断电保留 |
| ES8311 / 扬声器 | `main/demo_audio.c` / `demo_audio_tone()` | 生成 440 Hz 正弦波并播放 0.5 秒 |
| 麦克风 | `demo_audio_record_play()` | RAM 中录音 2 秒后回放，显示 PCM 峰值 |
| OV02C10 / CSI | `main/demo_camera.c` / `demo_camera_capture()` | 获取 5 帧 RGB565，报告尺寸与抽样校验值，释放帧缓冲 |
| C6 / Wi-Fi | `main/demo_network.c` / `demo_wifi_scan()`、`demo_wifi_connect()` | 扫描 AP、连接路由器、DHCP、有限次数重连 |
| RJ45 / IP101 | `demo_ethernet_start()` | 启动 P4 EMAC，异步报告 Link / DHCP IP |
| RS485 | `main/demo_io.c` / `demo_rs485_*()` | UART1 发送一行文本，等待最多 3 秒接收回复 |
| USB OTG Host | `main/demo_usb.c` / `demo_usb_start()` | 设备插入/拔出事件、读取 VID/PID/设备类别 |

摄像头例子已经包含完整采集过程，但没有做实时预览或 JPEG 编码。
USB 例子是枚举入口，不包含 U 盘文件系统、键盘 HID 或 USB 摄像头的类驱动。
蓝牙、OTA、Modbus、MQTT 等属于下一层协议，本工程没有把它们包装成“已经实现”的功能。

## 3. 引脚与总线分配

| 功能 | P4 引脚 / 连接 |
|---|---|
| 共用 I2C | SDA=7，SCL=8；触摸、ES8311、RTC、摄像头控制共享 |
| LCD | MIPI DSI 专用信号；RESET=0；背光=23 |
| GT911 | 原理图 INT=21、RST=22；当前厂商 BSP 不驱动这两脚，使用轮询和已有硬件复位状态 |
| 音频 | MCLK=13，BCLK=12，WS=10，P4 DOUT=9，P4 DIN=48，功放使能=11 |
| TF 卡 | SDMMC slot0；D0/1/2/3=39/40/41/42，CLK=43，CMD=44；LDO4 |
| P4 ↔ C6 | SDIO slot1，4 位、40 MHz；CLK=18，CMD=19，D0/1/2/3=14/15/16/17，C6 RESET=54 |
| RS485 | UART1 TX=26，RX=27；板级自动方向控制，无软件 RTS |
| 以太网 | MDC=31，MDIO=52，PHY RESET=51，PHY 地址=1；CLK=50，TXEN=49，TXD0/1=34/35，CRS_DV=28，RXD0/1=29/30 |
| BOOT / RESET | BOOT=35；RESET 是硬件复位，不当作软件按键读取 |
| 调试 | USB Serial/JTAG（本机 COM13）；UART0 TX=37/RX=38 也输出控制台日志 |

不要将 GPIO26 当作空闲 LED，也不要把 GPIO21 当作 BOOT。
**GPIO35 同时连接 BOOT 和 RMII TXD1。启动 Ethernet 后，程序停止按键检测，此时不要按 BOOT。**
软件互斥不能解除实际硬件连接；在以太网活动期间按下 BOOT 会拉低 TXD1。重新启动板子后才恢复按键测试模式。
厂商 BSP 内残留的 `BSP_USB_NEG=19` 等旧宏不代表本板高速 USB 接口引脚，不能照着配置 GPIO。
USB Host 示例通过 IDF 6 默认的高速 OTG PHY 初始化，不使用这两个旧宏。

## 4. 推荐试验顺序

1. 烧录后拖动背光滑块、点击按钮，检查触摸坐标；启动完成后短按 BOOT。
2. 点 I2C scan，再点 RTC read。RTC 未校时或电池没装时，可能返回时间无效，不等同于通信失败。
3. 插入 FAT 格式 TF 卡，点 TF read/write。示例绝不自动格式化，只删除本次创建的临时文件。
4. 点 Speaker tone，再点 Record + playback；接好扬声器，对麦克风说话。
5. 接好摄像头后点 Camera capture。该板目前使用 OV02C10，换型号需调整驱动和配置。
6. 点 Wi-Fi scan；填好账号后测试 Connect。连接成功表示取得局域网 IP，不等于已经验证公网可用。
7. 接网线后点 Ethernet start，观察串口中的 DHCP 地址。
8. 使用另一端 USB-RS485 适配器测试发送/接收；两端参数相同，A/B 对接并按设备要求共地。
9. 用合适的 OTG Host 转接线连接 USB 设备测试枚举。COM13 调试口仍用于日志，不用它代替 OTG 口。

测试错误显示在屏幕，详细记录通过串口输出。按钮点击后立即显示 Queued，已有任务执行时显示 Busy。
每次收到新状态会将结果区滚回顶部，避免之前的滚动位置让短提示看起来消失。
未接设备时报错或超时是预期情况。
USB 枚举后主机任务常驻；TF 挂载后保持挂载状态。断电前避免正在读写 TF 卡。

## 5. Wi-Fi 怎么配置（当前底座）

进入屏幕的“网络”页，点击扫描，从下拉列表选择热点，输入密码并选择“保存并连接”。
也可手动输入 SSID；键盘不包含中文输入法，中文热点通过扫描列表选择。
开机自动连接开关随“保存并连接”保存到 NVS。连接失败后可修改并重试。
以下 menuconfig 项来自旧示例，当前连接逻辑不再读取它们，请勿继续在其中填写密码。

```powershell
.\build.ps1 menuconfig
```

进入 `JC1060 hardware examples`，填写 `Wi-Fi SSID` 和 `Wi-Fi password`，重新 build / flash。
旧版本的密码可能仍在本地 `sdkconfig`；发布工程前应清理。
当前版本凭据保存在 NVS 的 `system/config_v1`，不在日志中输出密码，未启用 NVS 加密。

P4 应用直接调用 `esp_wifi_init()`、`esp_wifi_scan_start()`、`esp_wifi_connect()`。
`esp_wifi_remote` + `esp_hosted` 负责把请求经 SDIO 交给 C6。
本工程固定 Host `esp_hosted=2.11.7`，沿用厂商工程选用的系列。
此前板上 C6 报告 Hosted 2.3.0，与 Host 系列有版本差异；新工程联网功能仍需实机验证兼容性。
当前已实测 SDIO 初始化和 Wi-Fi 扫描成功，返回 8 个热点；连接路由器和长期稳定性仍未验证。
若出现固件版本告警、RPC 超时，应先核对 Host/C6 配套版本，不要把 C6 固件当 P4 程序烧进 COM13。

## 6. 在此基础上开发

### 添加自己的按钮和后台功能

当前版本请优先使用 [PLATFORM.md](PLATFORM.md) 中的应用模块注册和 `platform_submit()`。
以下为原示例接口说明；`actions` 现在只用于诊断页，任务与状态存储已迁移到 `platform.c`。

```c
// 1. 在一个新的 demo_xxx.c 中实现；在 main/CMakeLists.txt 的 SRCS 添加文件。
esp_err_t my_sensor_test(char *result, size_t capacity)
{
    // 初始化/读取传感器；失败返回 esp_err_t，结果写入 result。
    snprintf(result, capacity, "Value: %d", 123);
    return ESP_OK;
}
// 2. 在 demo.h 声明，并把 {"My sensor", my_sensor_test} 加到 demo_ui.c 的 actions。
// 3. 增加按钮数量时同时调整布局或改为可滚动容器。
```

按钮回调只投递任务；`platform.c` 的 `worker` 串行调用外设示例，`refresh` 定时刷新 UI。
不要在 LVGL 回调中录音、读卡或等待联网。其他任务操作 `lv_*()` 前应持有
`bsp_display_lock()`，完成后 `bsp_display_unlock()`；LVGL 自己的事件回调不必重复加锁。
`demo_post_status()` 可以跨任务使用，通过互斥锁保存最新结果和最近 8 条摘要，串口保留完整记录。

示例外设函数默认由单个 worker 串行调用，不是任意并发调用的通用驱动库。
增加多个业务任务后，需要给音频 codec、RTC、SD 文件和摄像头生命周期增加自己的互斥管理。
所有 I2C 外设复用 `bsp_i2c_get_handle()`，不要在同一引脚上再次新建另一套 I2C 驱动。

### 校准 RTC

```c
struct tm utc = {
    .tm_year = 2026 - 1900, .tm_mon = 10 - 1, .tm_mday = 30,
    .tm_hour = 0, .tm_min = 0, .tm_sec = 0, .tm_wday = 5,
};
// 将以上值替换为真实 UTC 时间，星期 0=周日，5=周五。
esp_err_t ret = demo_rtc_write(&utc);
```

只在用户校时或获得可信网络时间时写 RTC，不要每次启动覆盖成编译时间。
本工程约定 RTC 保存 UTC，显示北京时间时再加时区转换。
RX8025T 寄存器协议与 RX8025SA 不同，不能直接替换两者的驱动。
寄存器映射/有效标志参考 [ESPHome 的 RX8025T 实现](https://api-docs.esphome.io/rx8025t_8cpp_source)，
硬件型号依据随板原理图 U9。此处独立实现基础时间读写，不包含定时中断、闹钟或温补校准设置。

### 处理摄像头图像

在 `demo_camera_capture()` 的 `VIDIOC_DQBUF` 与 `VIDIOC_QBUF` 之间访问 `buffers[buffer.index]`。
RGB565 每像素 2 字节；实际长度使用驱动返回的 `bytesused`，不要假设与显示屏尺寸相同。
如果 LVGL 要在函数返回后继续使用图片，必须复制到自己持有的缓冲区，不能继续引用被释放的 MMAP 数据。
OV02C10 驱动保留在 `components/esp_cam_sensor`，不要简单替换为不含此传感器的旧注册表版本。

## 7. 验证范围与已知限制

- 以本工程目录中的 `VALIDATION.md` 为准区分编译验证与实机验证。
- 先前已经烧录运行的是 `lvgl_demo_v9`；不能把先前的结果当作此工程全部外设测试通过。
- 缺卡、缺摄像头、未接网线、RS485 无对端会超时或失败，界面继续运行。
- 厂商 BSP 内部少数初始化仍使用 `assert/ESP_ERROR_CHECK`；严重驱动配置错误不保证能自动恢复。
- 音频底层 codec API 为阻塞式；当前按固定两秒录音使用。若做长期录音，应改用环形缓冲和专门任务。
- 摄像头 DQBUF 超时为 3 秒；不接收图像时不会无限等帧。
- 若串口出现 `GPIO23 is not usable` 或 Boya generic flash driver 警告，需结合实际亮度/读写表现判断；先前 LVGL 启动也出现过这类提示。

## 8. 代码来源

`components/espressif__esp32_p4_function_ev_board` 从已成功运行 LVGL 的厂商 BSP 复制；
`components/esp_cam_sensor`、`components/espressif__ip101` 从厂商 `esp_brookesia_phone/components` 复制。
保留各组件原许可证和版权头。`main/` 是为本板整理的调用示例，不改变原资料目录和之前的 LVGL 工程。

## 9. 字体与 Wi-Fi 名称

`main/demo_text.c` 提供统一字体和 UTF-8 安全复制。英文字母/数字使用 Montserrat 16px，
缺少的字形从 `main/fonts/board_font_cjk_16.c` 查询。
中文字体以 LVGL 附带的 SourceHanSansSC-Normal.otf 为源，使用 lv_font_conv 1.5.3 生成，
字号 16px、4bpp，使用未压缩字库（按用户反馈已回退压缩试验）。包含源字体在 CJK 基本区、扩展 A、兼容区、全角标点以及指定拉丁扩展/标点区域中实际存在的字形。
这不是所有 Unicode 字符的字体；emoji 和更高的汉字扩展区不在此次字库中。
保留 `main/fonts/LICENSE-SourceHanSansSC.txt`。生成方法见 `tools/generate-font.ps1`，正常编译无需重新生成。

开启 `CONFIG_LV_FONT_FMT_TXT_LARGE=y`，避免大字库的位图偏移溢出。
`tools/generate-font.ps1` 已恢复为默认生成未压缩版本。
压缩试验及回退记录见 [FONT_COMPRESSION.md](FONT_COMPRESSION.md)，其中压缩数据不代表当前固件。
`demo_text_font()` 返回具有持久生命周期的字体副本，不修改 LVGL 的 const 全局字体。
新增控件如需显式指定字体，可调用 `lv_obj_set_style_text_font(label, demo_text_font(), 0)`。

SSID 最多 32 字节，可能不是 UTF-8。示例保留有效 UTF-8；非法字节和控制字符显示为 `\xNN`，
不把半个汉字或无效字节直接交给 LVGL。结果区可上下滚动，避免中文行高变大后把后续网络名称裁掉。
开机状态区有 `ABCDE... / 中文无线网络 / Café` 对照文本；串口执行 ASCII、中文、重音字母、UTF-8 边界自检。
扫描结果中的无线网络名称属于外部输入，显示缺字时先区分字体覆盖、编码与布局问题。

字体转换工具说明：[LVGL lv_font_conv](https://github.com/lvgl/lv_font_conv/blob/master/README.md)。

## 10. 按钮与 Wi-Fi 排查入口

按钮点击会记录 `ui: Clicked: ...`，投递成功记录 `ui: Queued: ...`，后台任务随后报告 Running。
按钮下方显示当前任务状态；底部浅色结果面板使用显式深色文字，可上下滚动查看完整结果。
这修复了普通 LVGL 容器继承主题深色文字、透明背景叠在深色屏幕上导致结果几乎不可见的问题。
Wi-Fi 会依次显示 C6/SDIO 初始化、station 启动、扫描中和扫描结果。
扫描使用完成事件与 15 秒超时；同时只能提交一个测试，避免连续点击排入多个长时间操作。

USB Serial/JTAG（COM13）支持发送一行 `scan` 后回车，交给与触摸按钮相同的硬件 worker。
用于区分界面事件问题与 C6/SDIO 问题，不需要改 SSID 或烧写 C6。
对应代码：`main/demo_console.c`、`demo_request_wifi_scan()`。

本板实读 GT911 坐标范围为 **1024×600**。旧 BSP 的日志声称 800×480 校准，但未注册回调，
该日志不代表实际硬件配置。现在 BSP 读取 0x8048..0x804b 的范围后进行映射，本板为 1:1，
并正确限制在 0..1023 / 0..599。不要直接用固定 800×480 的比例放大本板触摸坐标。
