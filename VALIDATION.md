# 2026-11-02 常用中文字库精简（已编译，未烧录）

用户接受不常见字符缺字。保留 GB2312 一级3755汉字、当前main源码额外使用字符共3757汉字，另保留常用拉丁字符、标点、箭头与全角符号，总计4275有效字形。仍为原SourceHanSansSC字体、16px、4bpp、不压缩。
生成工具：tools/collect-font-characters.py 与 tools/generate-font.ps1；tools/font-characters.txt为生成字符表。后续新增页面中文后需重新生成字库；额外字符可写入tools/font-extra-characters.txt（UTF-8）。非保留的生僻字、繁体字等可能显示缺字框。
verify-font-subset.py 比对精简前全量字库：3757汉字覆盖通过，4275保留字形的间距、尺寸、偏移和位图逐一一致。位图3379494→473875字节；字形记录28632→4276（包括空字形）。
构建通过，日志tmp/font-subset-build.log；镜像0x43a410字节，8MiB应用分区余0x3c5bf0，约47%。未修改天气图标、分区、C6或用户数据。本轮未烧录；实机字形显示仍待验收。
修改前完整字库和生成脚本备份：tmp/font-subset-before。

---
# 2026-11-02 基础桌面开发（未烧录）

按概念图实现 1024×600 浅色桌面：时间/日期/农历与真实天气摘要、6×2 应用卡片、底部桌面/系统信息/诊断导航。复用现有设置、网络、应用生命周期和后台服务。
已有应用入口接入原描述符；音乐、麦克风、HC-08 显示“待迁移”且禁用。系统信息独立成页。保留原应用内容区域尺寸 976×482，避免改变已迁移应用的布局契约。
本轮没有打开串口、复位或烧录，没有修改 BSP、C6、分区、字库或持久化配置格式。图标使用现有 LVGL 字体符号，非概念图位图。
源码修改前备份：tmp/desktop-base-before。构建日志：tmp/desktop-base-build.log。
待实机验收：中文与符号显示、触控区域、桌面反复进出、所有应用返回、键盘遮挡/关闭、调暗与恢复、网络/天气状态更新。旧版记录不代表本轮验收通过。

---
# 当前版本 0.5.0-dev：USB 键鼠蓝牙桥接

已构建并烧录COM13，写入hash通过；镜像6,347,392字节，SHA256：027D9D152EC6B2D6F0BFE1FC0A780FDB2CE517BCFA879FDE68AD1731FBE8A118。
未擦除用户数据，未更新C6，字库仍为原未压缩版。启用P4 NimBLE Host、Hosted SDIO VHCI和USB Hub。
键盘/鼠标生产算法C测试、USB快捷键回归测试和Hosted2.5.2版本边界测试均通过。
P4实机观察：桥接ESP_OK、HOGP服务启动、USB HID Host启动ESP_OK、广播TouchDesk KeyMouse。
从端RPC版本返回2.3.2，走旧版控制器自动启用路径；不发送2.5.2新增控制器启停RPC。
实机追加检查：桥接Host→USB快捷键Device→桥接Host切换均ESP_OK；HID接口/驱动正常停止与重启，未捕获panic/assert。日志：tmp/ble-bridge-switch-runtime.log。
用户暂无键鼠/OTG转接头，实际枚举、电脑配对、加密订阅和输入效果未验收。不能将广播成功等同于完整桥接实测。
使用说明见USB_BLE_BRIDGE.md，迁移细节和日志见MIGRATION.md。

以下为历史记录。
# 当前版本 0.4.0-dev：USB 快捷键迁移

已构建并烧录 COM13，写入hash通过；未擦除NVS/storage，未更新C6，未修改字体及分区。
镜像6,147,328字节，SHA256：91D9C8FCF6C37E5D57BB73ED0D09800CEDF6391A63E1F84A5DAB04C24DDECAF4。
USB生产worker的主机测试通过，包含满队列释放、断线不重放、失败保留占用与媒体键等。
实机序列 HID start→stop→Host→HID start→stop→Host 均ESP_OK，无捕获的panic/assert。
最后留在USB Host枚举状态；从应用进入USB快捷键会自动关闭Host再启动Device。
未实际连接电脑验证枚举/按键，不应视为HID全链路验收；USB→BLE、ESP-NOW仍未完成。
详情见MIGRATION.md；日志在工作区tmp/usb-shortcuts-*.log。
扬声器确认需外接，用户要求搁置声音排查。

以下为历史记录。
# 音频无声排查（当前镜像）

用户反馈小智和诊断提示音均无声；小智状态日志曾为 SPEAKING，TX266/RX234，说明收到并处理回复不等于实际发声。
修复 ES8311 重复驱动对象：speaker codec_mode 改为 BOTH，应用将同一 IN_OUT 句柄用于采集/播放，不再调用第二次 microphone codec 初始化。
播放音量从35调为60，并增加只读的功放/寄存器状态日志。音频模式主机测试、构建通过，烧录COM13写入hash通过。
当前镜像 SHA256：4A05B33E6664B7294A16CE4909FCC3FCD1E39077186CD2065732D11BD571CDA6。
两次提示音均ESP_OK；读回PA=1、DAC31=00、volume32=9e、power12=00。用户仍确认没有声音，不能宣称修复完成。
本地原理图 SPEAKER 区为 ES8311→NS4150→CN1（SPEAKER_P/N），需确认实体扬声器连接与供电。
用户不确定扬声器/接口位置，未曾验证原厂程序声音。下一步需要背面照片/实体连接确认，之后继续查I2S及功放模拟链路。
日志：工作区 tmp/audio-single-codec-build.log、audio-single-codec-flash.log、audio-single-codec-runtime.log。
BSP显示/触摸、分区、字体、NVS/C6未改。此处仅针对BSP音频codec模式作修复。

以下为历史记录。
# 当前迁移版：0.3.0-dev，已烧录 COM13

应用镜像 6,112,448 字节，SHA256：BD4096FBBED499F0211EAD9BE504BC2D49148B32E0813D4F82450BF7B7935425。
修复版构建和烧录均返回 0，写入 hash 校验通过；工具随后复位。未擦除 NVS/storage，未更新 C6，中文字库 hash 与原版一致。
已移除底部常驻日志区，详细记录位于诊断页。

本轮已确认：
- 三组原串口 C 测试通过：帧编解码/碎片噪声流、512 历史/序号回绕、时间窗口曲线。
- tests/voice_backend_test.py 通过，覆盖格式/分块/切换/占用及打开失败恢复。
- P4 实机：小智初始化 ESP_OK、available=1；未启动云端语音会话。
- Wi-Fi 连接 ESP_OK，取得局域网 IP；天气 city=0 HTTP=200，解析 ESP_OK。
- 板载提示音、两秒录音回放 API 均 ESP_OK。PCM 峰值达到 32768，不能据此认定录音音质正常。
- 初版小智读取 ESP_MAC_WIFI_STA 返回不支持，已改为 P4 efuse 出厂 MAC，修复版初始化通过。
- 本次有限串口观察未捕获 panic/assert/反复重启，不能代替长期稳定性验收。

仍未完成：USB 快捷键、USB→BLE 键鼠、C6 ESP-NOW 扩展；其余应用也还有旧界面和功能差异，详见 MIGRATION.md。
有线 RS485 未与对端完成双向实测；小智绑定/实际语音、天气全部界面与长期联网仍待验证。
日志位于工作区 tmp/migration-voice-build-fix.log、migration-mac-fix-build.log、migration-mac-fix-flash.log、migration-mac-fix-runtime.log。

以下为历史记录。
# 第一批迁移固件已烧录

用户授权后已通过 build.ps1 flash COM13 烧录，命令返回 0，写入数据 hash 校验通过，工具随后执行硬件复位。
应用镜像 5,460,368 字节，SHA256：18B136E5332B7D2F51E38A46AC4319844DD28AD38A6010BEABC527686A40A53D。
写入项为 bootloader、原布局分区表、P4 应用；未擦除 NVS，未写 storage 或板载 C6。
日志：工作区 tmp/migration-first-flash.log。未另开串口监视，启动、界面和应用行为仍待实机验收。
以下“未烧录”描述为此次烧录之前的历史状态。
# 迁移第一批（0.2.0-dev，未烧录）

2026-10-30：在原底座增加集中注册表、计数器和时钟页，迁入旧系统串口帧/历史/曲线算法。
本批只进行源码和构建验证，设备仍为此前未压缩字体底座。
构建日志：工作区 tmp/migration-first-build.log。功能范围与待办见 MIGRATION.md。

- ESP-IDF 6.1 / LVGL 9.5 构建通过，新增模块编译、链接成功。
- 原有 C 测试已迁入 tests/serial；本机缺少可用的 Windows 原生 C 运行工具链，尚未执行，不能记为测试通过。
- BSP、分区和字体生成脚本没有主动修改；字库文件与本批备份 SHA256 相同。
- 待实机验证：三列应用入口、反复进入/退出时钟无悬空 timer、计数边界及保存断电保留。
- 时钟未接 SNTP/RTC；无有效系统时间时应显示“尚未校时”。
- 串口算法没有接入物理 UART/ESP-NOW，不表示无线迁移完成。

以下保留原版本历史记录。
# 验证记录

## 最新状态：恢复未压缩中文字库

2026-10-30 用户实机反馈后要求回退。已恢复原未压缩字体和原生成脚本，
重新构建通过，生成镜像与压缩前备份 SHA256 完全一致：
`5BE2AA4B5E5333CDC977E79BEAA75B9BE0707D02B5D92882E7B9576BF8D28D49`。
当前固件 5,447,152 字节，应用分区剩余约 35%。以下压缩记录仅为历史试验。
构建日志：工作区 `tmp/font-compression/rollback-build.log`。
已通过 esptool 将原备份应用镜像写回 COM13 的 `0x20000`，写入 hash 校验通过并硬件复位。
未擦除 NVS，未改分区及 C6 固件。烧录日志：`tmp/font-compression/rollback-flash.log`。

## 最新源码：中文字库无损压缩（已烧录）

2026-10-30 按用户要求保留全部字形与 4bpp 灰度，改为 RLE + XOR 预处理压缩。
全量 28,631 字形、6,756,376 像素与原版一致；构建及镜像完整性检查通过。
当前 build 镜像为 4,925,584 字节，比已烧录底座节省 521,568 字节（9.58%）。
随后按用户要求通过 `build.ps1 flash COM13` 烧录，返回 0，写入 hash 校验通过。
通过 USB `scan` 检查运行：C6 SDIO 握手与扫描成功，返回 ESP_OK 和 8 个热点，含中文 SSID。
观察期间未见 panic 或反复重启；中文显示实际观感与速度仍待验收。
日志：工作区 `tmp/font-compression/flash.log`、`runtime.log`。
详细数据和回退方式见 [FONT_COMPRESSION.md](FONT_COMPRESSION.md)。
下方底座镜像大小及 hash 是上一次未压缩、已烧录版本的历史记录。

## 当前源码：应用底座 0.1.0

2026-10-30 从硬件示例升级为应用底座。本版新增首页/网络/设置/诊断、应用生命周期、
共享后台任务、NVS 系统配置、扫描热点选择与屏幕配网、调暗及最近运行记录。
开发说明与逐项实机验收清单见 [PLATFORM.md](PLATFORM.md)。

本版随后已按用户要求烧录 COM13，写入 hash 校验通过。**下方历史章节中的烧录、显示和扫描结果属于旧硬件示例，不能作为新版本全部功能通过的依据。**
本版新的配置断电保留、界面导航、自动连接及断线恢复均待实机验证；外设验证边界不扩大。

本版构建验证：

- `build.ps1 build` 通过；最终增量构建的修改源码无警告或错误。
- 应用镜像 5,447,152 字节（`0x531df0`），8 MB 分区剩余 `0x2ce210`，约 35%。
- esptool 5.6.0 image-info：ESP32-P4、revision 3.1–3.99、16 MB / 80 MHz / DIO，checksum/hash 均有效。
- 镜像 SHA256：`5BE2AA4B5E5333CDC977E79BEAA75B9BE0707D02B5D92882E7B9576BF8D28D49`。
- 外壳版本为 0.1.0；保留原工程名 `jc1060_hw_starter`，IDF 镜像 app version 仍为默认值 `1`。
- 最终构建日志：工作区 `tmp/platform-build-verified.log`。

本版烧录及运行观察：

- `build.ps1 flash COM13` 返回 0，esptool 写入 hash 校验通过，随后硬件复位。
- 串口采集约 20 秒；启动早期日志存在缺失，不能据此宣称完整的启动日志验收。
- 采集到本版 `system` worker 执行“Wi-Fi 扫描”，C6 SDIO 握手、station ready、扫描返回 `ESP_OK` 和 8 个热点，包含中文 SSID。
- 采集期间未见 panic/assert/反复重启；这不替代长期稳定性测试。
- C6 版本差异告警仍存在；连接路由器、NVS 配置断电保留及新界面观感仍待验收。
- 日志：工作区 `tmp/platform-flash.log`、`tmp/platform-startup.log`。

## 历史记录：硬件调用示例

日期：2026-10-30。

## 已完成

- Windows 本机 ESP-IDF v6.1 / RISC-V GCC 15.2.0 全量编译成功。
- 修改 GPIO35 互斥及界面提示后，通过项目 `build.ps1 build` 再次编译成功。
- 新增 `main/*.c` 在最终增量构建中没有编译警告或错误。
- 全量构建的第三方组件仍有弃用 API / 未使用函数等警告；厂商 OV02C10 Kconfig 有重复定义提示，没有隐藏这些警告。
- 已运行 `idf.py save-defconfig`，保存 `sdkconfig.defaults`。
- esptool 5.6.0 检查应用镜像：ESP32-P4，最低 revision 3.1，最高 3.99，Flash 16 MB / 80 MHz，镜像 checksum/hash 均有效。
- 当前应用大小：5,437,424 字节（0x52f7f0）；8 MB 应用分区剩余约 35%。
- 当前应用 SHA256：`EC60D879584DD96B00FABBF3B7BAA5E703F9B3103A13BB51B17450BFDA43E057`。
- 引脚与原理图/厂商配置复核：GPIO35 同时连接 BOOT 和 RMII TXD1，代码在启动 Ethernet 前暂停按键检测，初始化失败时恢复。

## 烧录与实机启动验证

随后已按用户要求烧录至 COM13，esptool 写入校验通过。
复位后读取 20 秒启动日志，确认工程名 `jc1060_hw_starter`、ESP-IDF v6.1、32 MB PSRAM 自检通过、
JD9165 LCD 与 GT911 触摸初始化成功、背光设置 75%，程序输出 `Ready`。
观察期间未发现 panic 或反复重启。用户已确认新版浅色结果框和 Wi-Fi 网络列表可见；其他外设仍需逐项操作验收。

| 项目 | 当前状态 | 实机验收方式 |
|---|---|---|
| 本工程启动、界面、触摸、背光 | 启动、真实按钮事件和结果显示已验证；亮度交互待验收 | 显示 12 个按钮，触摸坐标变化，亮度可调 |
| BOOT / GPIO35 | 编译通过，待测试 | Ethernet 启动前短按，计数增加；启动 Ethernet 后不要按 |
| I2C / RTC | 编译通过，待测试 | 扫描地址；RTC 有效时间递增，掉电标志正确处理 |
| TF | 编译通过，待插卡测试 | 写入回读一致，临时文件清理，不格式化 |
| NVS | 编译通过，待测试 | 点击计数，重启后再次读取仍保留 |
| 音频 | 编译通过，待扬声器/麦克风测试 | 有提示音，2 秒录音可回放 |
| OV02C10 | 厂商驱动已参与构建，待采集 | 获取 5 帧并报告尺寸/字节数；画质需另加预览验收 |
| Wi-Fi | SDIO 初始化、C6 station 和扫描实测通过；连接待验收 | 填写 SSID/密码连接并取得 IP |
| Ethernet | 编译通过，待接网线测试 | Link up + DHCP IP |
| RS485 | 编译通过，待对端测试 | 对端收到文本，板端收到回复 |
| USB OTG | 编译通过，待接 USB 设备测试 | VID/PID 和插拔事件正常 |

现有 C6 曾报告 ESP-Hosted 2.3.0，此工程 Host 为 2.11.7，需实测兼容性。
不把“Host 编译成功”标记为“C6 联网成功”。

## 按钮与扫描实机排查

- 用户反馈触摸坐标变化，但按钮看起来没有反应。
- 增加点击即反馈、Busy 提示、状态区自动回顶、worker 创建失败报告及事件日志。
- GT911 实读范围 1024×600，与屏幕相同，排除固定 800×480 这一旧日志所暗示的配置。
- `starter-input-diagnostic.log` 捕获真实触摸按下/松开/点击，包括两次 Wi-Fi scan 与一次 Camera capture。
- 同次测试中 USB `scan` 通过相同 worker 触发扫描，C6 SDIO 握手、station 初始化及扫描完成。
- 回传 `Wi-Fi scan: ESP_OK` 和 8 个热点。用户在扫描期间再次点击会立即提示 Busy，不重复提交。
- C6 版本差异告警仍存在，但此次基本扫描已验证可用；尚未验证连接路由器或其他 Wi-Fi 功能。
- `starter-wifi-repeat.log` 进一步捕获用户实际点击后的 `Clicked`、`Queued` 和 `Wi-Fi scan: ESP_OK`，确认触摸按钮到扫描结果的链路可用。
- 用户仍看不到变化后，定位到结果容器的主题深色文字配透明背景，叠在深色屏幕上导致低对比度。显式改为不透明浅色背景 `#f2f6fc` 和深色文字 `#15283f`，并增加独立任务状态行。
- 最终构建及 COM13 烧录校验通过；`starter-results-diagnostic.log` 记录实际文字颜色 `ff15283f`、背景 `fff2f6fc`、不透明度 255，扫描再次返回 `ESP_OK` 和 8 个热点。
- 用户随后明确确认“浅色框和网络列表都能看到”，结果区显示问题已验收。没有把 Camera capture 或 Wi-Fi connect 的点击事件记作这些功能通过。

构建日志位于工程外：
`C:\play\esp32p4-esp32c6\tmp\starter-build.log`、`starter-final-build.log`。
烧录/启动日志：同目录 `starter-flash.log`、`starter-startup.log`。

## 字体修复验证

- 原配置只有 Montserrat 英文字体，未启用中文后备字体；LVGL 自带 CJK 示例字体只含有限的字符集合。
- 已生成包含 28,631 个字形的思源黑体 16px 后备字体，保留 OFL 许可证和生成脚本。
- 已启用大字体偏移格式，统一使用 Montserrat 16px + CJK 后备字体；扩展了结果区的滚动布局。
- SSID 有效 UTF-8 保留；非法字节/控制字符转为可读的 `\xNN`。结果字符串不截断 UTF-8 字符。
- 编译与 COM13 烧录校验通过。复位后的 20 秒日志确认 LCD/触摸/PSRAM 正常，无 panic 或循环重启。
- 板上输出：`font: PASS: 119 ASCII/CJK/Latin glyphs; UTF-8 boundary and invalid-byte checks`。
- 已验证字形查找与编码边界；实际屏幕观感和现场 Wi-Fi 名称需用户再扫描确认。
- 日志：`tmp/starter-font-build.log`、`tmp/starter-font-flash.log`、`tmp/starter-font-startup.log`（相对工作区根目录）。








2026-11-02 本轮增量编译与链接成功，构建日志未发现 warning/error；未进行实机验收。

2026-11-02 用户明确授权后通过 COM13 烧录基础桌面，flash 返回 0，写入 hash 校验成功并由工具复位。随后采集约25秒日志，捕获 Wi-Fi 获取 IP 和 ESP_OK。启动早期日志不完整，不能据此确认显示/触摸初始化与完整启动验收。未更新板载 C6，未擦除 NVS/storage。日志：tmp/desktop-base-flash.log、tmp/desktop-base-runtime.log。桌面视觉与触摸待用户确认。

2026-11-02 天气时钟横屏改版：纸白墨绿、大时钟（可切秒）、日期/农历、真实当前天气、未来5小时与未来3天、城市选择、刷新、显示设置。恢复旧系统16类立体图标，以128px ARGB8888静态资源适配LVGL9；桌面天气入口使用天气图标。修正风速显示为数据源的m/s。保留底部系统导航。构建日志tmp/weather-layout-build.log，源码备份tmp/weather-before。此轮尚未烧录，布局、图标缩放、秒数点击、城市切换及预报切换需实机验收。未修改分区、BSP、C6或字体。

2026-11-02 天气新版按用户要求烧录COM13成功，flash返回0，hash校验通过并复位。日志tmp/weather-layout-flash.log。应用7723360字节，8MiB分区余665248字节。16张128px ARGB8888天气图标共1048576字节；同像素PNG内存编码测算280563字节，尚未实施压缩。

2026-11-02 精简字库版经用户授权烧录COM13成功：flash返回0，写入hash通过并复位。后续日志捕获SNTP校时成功、证书验证通过、天气HTTP200/ESP_OK。早期启动日志不完整，字体启动自检未捕获，不能据此认定屏幕字形已实机验收。未擦除NVS/storage，未更新C6。日志tmp/font-subset-flash.log与tmp/font-subset-runtime.log。

2026-11-02 无线串口全屏改版（未烧录）：应用描述符新增fullscreen能力，外壳进入时隐藏标题/状态栏/底部导航，退出恢复。无线页面原生1024x600布局，取消旧480x320整体缩放；10个字段两行五列且显示data编号/类型，四个状态按钮、全屏数字编辑器、756x350曲线及侧栏控件。顶部返回桌面始终可达（编辑器中先取消编辑）。切到接收/曲线会关闭20ms持续发送，已排队传输仍依原协议处理。退出清理standalone_ctx；分配/创建失败请求返回。未改变传输协议、C6、分区。字体覆盖与原字形逐像素比对通过。最终编译成功，日志tmp/wireless-fullscreen-build-final.log。待实机验收：全屏进出、发送/接收/曲线、数值编辑/取消、暂停/清空、Wi-Fi恢复和实际无线收发。修改前备份tmp/wireless-fullscreen-before。

2026-11-02 无线串口全屏版经用户授权烧录COM13成功，flash返回0，hash校验通过并复位。后续15秒日志捕获SNTP校时成功、证书验证通过和天气HTTP200/ESP_OK。未执行无线页面操作或无线收发测试，早期启动日志不完整，实机布局与触摸待用户验收。未更新C6，未擦除NVS/storage。日志tmp/wireless-fullscreen-flash.log、tmp/wireless-fullscreen-runtime.log。

2026-11-02 ESP-NOW当前实机双向验收：COM13 P4、COM12外接C6 fw1.3.1-p4-peer、COM10 PL2303GC业务串口。已有配套固件在线，无需再次烧录。本轮COM10以921600 8N1发1000个48字节帧（每10ms一帧），P4 rx_frames增1000、rx_bytes增48000、末帧及累计CRC32C=2398081033正确；反向通过now send调用实际UI发送路径发20帧，COM10收到960字节逐字节一致，最终ACK增20。peer dropped_bytes/uart_errors/crc_errors/fault均0，queued/pending均0；mac_fail累计1，未影响端到端完整性。页面重开3次仍在线，最终停在接收页。重开后曲线回填512条、history_lost=488为上游历史环形缓存限制，不是无线丢帧。未执行长期或极限吞吐验收。日志tmp/espnow-current-probe.log、tmp/espnow-20261102-result.log、tmp/espnow-20261102-e2e.log；测试脚本tmp/test-espnow-20261102.py。

2026-11-02 P4无线曲线性能优化（未烧录）：用户确认卡顿在P4。现场累计P4 rx_frames12405、bad_tail19、discarded704；外接C6 rx_bytes621504、accepted548144、dropped73360、retry_packets511，表明存在入口过载，不能仅归因于绘图。建议10ms发送周期再验收。代码取消曲线页对隐藏收发控件的刷新、相同标签不重复设置、采集/曲线定时器100→50ms、非曲线字段仍100ms刷新；1000点时间网格按192组极值包络输出最多384点，保留网格分组峰谷，含非有限值的组断线显示，原始历史容量与无线协议不变。实际FPS与端到端性能未实测，20Hz仅为定时目标。源码备份tmp/curve-performance-before，最终构建日志tmp/curve-performance-build-final.log。

2026-11-02 曲线性能优化版经用户授权烧录COM13，flash返回0，hash校验通过并复位。通过控制台进入无线曲线页，ESP-NOW切换ESP_OK，online=1，RSSI=-53，当前0接收帧，等待用户发送验证。未占用COM10、未更新C6。尚无实际绘图FPS或持续数据流性能结论。日志tmp/curve-performance-flash.log、tmp/curve-performance-runtime.log。

2026-11-02 ESP-NOW入口限流：新增dev/TouchDeskHost确认限流上位机，COM12反馈驱动COM10额度，最多1920字节在途，状态600ms过期停发，掉线暂停、错误/会话改变锁止，按逻辑波形相位延后生成。外接C6更新1.3.2-credit-status仅将报告周期1s→100ms；COM12应用区0x10000烧录hash通过，原应用读回1MiB保存tmp/flow-control-before/com12-app-readback.bin，未改分区/NVS。P4和板载C6未改。4项主机测试通过；硬件复测3000帧144000字节8.891秒，337.42帧/s，CRC/末帧匹配，C6丢弃/UART/CRC错误0、P4帧尾/解析错误0、history_lost0，反向10帧逐字节匹配。第一轮同样完成3000帧与CRC检查但旧错误计数跨会话清零使测试基准断言失败，已改正测试后复测。证据tmp/credit-e2e-result.json、tmp/credit-e2e-records.json、tmp/c6-credit-flash.log。新版上位机已启动供用户测试，未自动开启波形发送。长期断线/持续1kHz实时交付未作保证。

2026-11-02 第二轮吞吐优化：外接C6 1.3.3-fast-credit，20ms精简JSON数组信用快照+1s完整状态，COM12 app写入校验通过；主机批量写入最多20帧，最小5ms调度，仍1920字节在途。5项生产主机逻辑测试通过。500/800/1000目标各3000帧实际494.89/657.61/631.58帧/s，CRC/末帧和反向10帧通过。800首次测试因P4统计100ms发布延迟误判，等待统计稳定后复测通过。30000帧长测45.766s，655.51帧/s，1440000字节CRC匹配，C6 dropped/UART/CRC错误0，P4 bad_tail/discarded0，UI history_lost0；有无线重传但已完整确认。并未实现持续实时1kHz交付。结果tmp/fast-credit-long-1000-result.json，分档结果tmp/fast-credit-*-result.json。修改前备份tmp/fast-credit-before，P4/板载C6未改，COM12只写应用。新版上位机已重新打开，连续发送未自动开启。

2026-11-02 吞吐瓶颈验证（未改/烧录固件，默认额度仍1920）：测试程序独占COM13/12/10，7组各10000帧，共70000帧均数量/字节/末帧/CRC匹配、零新增C6丢弃/UART/CRC错误、零P4帧尾/解析错误、零UI历史缺口。无固定5ms节奏：curve窗口1920/3840/7680分别813/836/858帧每秒，ACK观测中位54/101/184ms，重传增2/1/295；1920接收页761帧/s。仿主机5ms批量节奏：1920/3840/1920分别859/852/831帧/s，ACK观测中位42/100/52ms，p95约66/121/68ms，平均无线包447/427/430字节；额度增大收益不明显且时延增加。20ms反馈间隔实测中位20.2–20.3ms、p95约22ms。时延从电脑write前到COM12观测ACK，含USB/反馈发布延迟，不是纯RF RTT。测试程序的调度与真实Qt界面不同，数值不能等同上位机实测FPS；没有分段打点，尚不能断言SDIO为唯一瓶颈。保留1920默认值，下一步建议测聚包与逐包RPC/ACK开销。证据tmp/throughput-validation.json、tmp/throughput-batched-validation.json。

2026-11-02 聚包超时实机对照：外接C6将FLUSH_US从5000改10000，测试版1.3.4-aggregate10编译并烧录COM12。1920字节额度、P4曲线页、5ms主机批量节奏，3组各10000帧得到746/758/842帧每秒，平均无线包854/904/814字节，ACK观测p95约79–82ms；没有稳定提速。已恢复5ms源码并重新编译/烧录，设备版本实测1.3.3-fast-credit。首次回退因复制文件保留旧时间戳导致Ninja复用测试版，版本断言在发送前发现；更新时间戳重新编译后回退成功。回退2组各10000帧为814/857帧每秒，ACK观测p95约74/63ms，重传增量均0。实验共50000帧，数量/字节/CRC/末帧均匹配，新增丢弃/UART/CRC/解析错误及UI历史缺口均0。保留5ms和1920字节额度，P4与板载C6固件未改。测试脚本吞吐不等于Qt界面实测，也未达持续1000帧/s。证据tmp/aggregation-10ms-validation.json、tmp/aggregation-rollback-validation.json、tmp/aggregation-rollback-rebuild.log、tmp/aggregation-rollback-reflash.log。

2026-11-03 触屏快捷键蓝牙扩展（未烧录）：app_keys新增USB/BLE切换、120秒配对入口、重新启动和键鼠就绪状态，复用TouchDesk KeyMouse配对记录与HOGP服务。快捷键与鼠标点击/移动通过BLE发送；按下/释放在同一临界区检查两条队列容量，未就绪或忙碌时拒绝。退出/切换释放输入，关闭USB键鼠桥接转发。现有蓝牙描述符无Consumer报告，音量三按钮在BLE模式禁用并标明仅USB可用。压缩按钮间距，最后一排结束于510px。P4最终编译链接成功，日志tmp/touch-ble-build-final.log未发现warning/error；尚未进行实机配对、按键效果、布局、断线重连及ESP-NOW共存验收。未改C6固件，未打开串口或烧录。本轮源码备份tmp/touch-ble-before。

2026-11-03 触屏蓝牙快捷键版按用户要求烧录COM13完成，flash返回0，写入hash校验通过并复位。应用4345856字节。复位后串口采集确认C6初始化、Wi-Fi获取IP、SNTP校时及天气HTTP200/ESP_OK。串口已关闭。未进行蓝牙配对或向电脑发送按键，实际配对/输入/断线释放仍待实机验证。未烧录外接或板载C6。日志tmp/touch-ble-flash.log、tmp/touch-ble-startup.log。

2026-11-03 蓝牙配对DHKey失败修复试验：现场日志tmp/touch-ble-pair-retry.log反复出现1035（本机SMP DHKey检查失败）、encrypted=0/bonded=0。上游esp-hosted-mcu issue241关联esp-idf提交9fd7cb7e606修复TinyCrypt ECC字节序，本机SDK esp_tinycrypt_port.c仍为修复前实现。项目sdkconfig及defaults启用CONFIG_BT_NIMBLE_CRYPTO_STACK_MBEDTLS，保留SC与原安全策略，未改全局SDK。完整编译成功，COM13烧录返回0/hash通过；生成配置确认MbedTLS启用，启动后天气HTTP200。尚待电脑配对复测，不能宣布已解决。备份tmp/ble-crypto-before，日志tmp/ble-crypto-build.log、tmp/ble-crypto-flash.log、tmp/ble-crypto-runtime.log。

2026-11-03 用户确认MbedTLS版蓝牙连接成功。随后实现触屏快捷键启动默认BLE（本次运行手动切到USB后保留选择至重启），新增Consumer Control报告ID3/16bit输入及加密通知，支持音量减0xEA/加0xE9/静音0xE2。复用有界报告队列，按下/释放成对入队；断线、切换、退出、协议切换清理输入；屏幕增加音量就绪状态。HID描述符解析检查通过：鼠标6B、键盘8B、LED1B、媒体2B；最终编译链接成功，tmp/ble-media-build-final.log未发现warning/error。未烧录、未实测Windows音量变化；新增GATT报告可能需要电脑删除旧配对后重新添加。备份tmp/ble-media-before。

2026-11-03 默认蓝牙与媒体键版本按用户要求烧录COM13，flash返回0，写入hash校验成功并复位。烧录后15秒串口采集未收到输出，不能据此确认启动或媒体键实机功能；串口已关闭。尚待用户进入触屏快捷键验证音量就绪、音量加减与静音。未更新C6。日志tmp/ble-media-flash.log；采集文件tmp/ble-media-startup.log为空。

2026-11-03 用户确认快捷键页面按钮太小。改为1024x600全屏，自带返回桌面；20个快捷键独立页，按钮218x44→240x72；鼠标/音量9个按钮独立页324x124，顶部按钮切换页面。主操作文字使用已有中英字形统一1.5倍显示，未增加字库；默认蓝牙及发送逻辑保留。静态布局边界核对：快捷键最右1006、最下580，控制页最右1008、最下576，无超出屏幕。编译成功且无编译warning/error，尚未烧录或实机视觉验收。备份tmp/keys-before-large.c，日志tmp/keys-large-build.log。

2026-11-03 全屏大按钮快捷键版本按用户要求烧录COM13完成：flash返回0，写入hash校验通过并由工具复位。烧录后15秒串口采集无输出，尚不能确认启动/布局/蓝牙实机行为，串口已关闭。未更新C6。日志tmp/keys-large-flash.log；tmp/keys-large-startup.log为空。

2026-11-04 小智触屏连续聊天入口：新增连续聊天按钮调用既有xiaozhi_continuous_start，保留连接/手动说话/发送语音/停止。依据会话状态禁止忙碌时重复启动、连续模式手动发送；显示连续模式提示。退出沿用xiaozhi_leave，停止沿用cancel关闭连续模式与会话；既有后端在TTS队列排空后自动恢复录音。修正手动模式提示为点击发送而非松开。最终编译通过，日志tmp/xiaozhi-continuous-ui-build-final.log。未烧录、未开启麦克风或联网对话，连续多轮与停止/断网实机验收待完成。备份tmp/xiaozhi-before-continuous-ui.c、tmp/xiaozhi-service-before-ui-hints.c。

2026-11-04 小智连续聊天版本按用户要求烧录COM13完成，flash返回0，写入hash校验通过并由工具复位。后续日志可见证书验证通过及天气HTTP请求ESP_ERR_TIMEOUT，说明应用已运行但此次天气网络请求超时；未进行小智连接或录音测试。串口已关闭，未更新C6。日志tmp/xiaozhi-continuous-flash.log、tmp/xiaozhi-continuous-startup.log。

2026-11-05 网络恢复后只读采集COM13约180秒，日志tmp/xiaozhi-repeat-network-restored.log。用户反馈本轮未复现重复播报；记录播报状态7次、回答结束7次、I2S未启用通道disable错误28次。错误与重复播报的因果未建立，现有日志不包含音频内容指纹，不能排除一次播报内部重复。未修改/烧录固件，串口已关闭。

2026-11-13 P4无线串口页面修改（未烧录）：状态按钮从4扩展到8，一行排列，data1 bit0~bit7分别映射状态1~8，数组/收发刷新/点击绑定同步扩展；保留发送态与收到状态指示点的原有语义。按钮宽116、水平步长124，最右1004，不重叠不越界。
10项数据值统一使用Consolas 40px ASCII子集等宽字体wireless_mono_40，所有字形advance22px，右对齐，6字符宽132px小于标签168px。新增wireless/value_display.h，普通十进制格式、不使用科学计数法，显示最多前6个字符（符号和小数点计入）；末尾孤立小数点移除；如1234567显示123456、-123456显示-12345。显示前格式化到6位小数并去尾零，极小值可能显示0；编辑框及48字节协议保留完整原值和精度，收发协议不变。曲线页及其它应用未改。
备份tmp/wireless-eight-before-20261113；字体由本机Consolas通过现有lv_font_conv生成。最终构建日志tmp/wireless-eight-build-final.log，构建成功，无编译warning/error；应用4433248字节。原生C显示函数测试覆盖边界样例和41011个float32位模式，最长6字符且无e/E；字形advance和8按钮静态边界检查通过，脚本tmp/test-wireless-display.py。
仅源码与构建验证，尚未打开COM13/烧录/实机屏幕验收；未修改外接C6、板载C6或电脑上位机。当前外接C6的无线协议与P4仍需按实际独立工程核对，不能由本次UI修改推断互通。

2026-11-13 八状态按钮与六字符等宽数值版经用户授权烧录COM13成功。预烧录重建通过；esptool识别ESP32-P4 revision v3.2、MAC80:f1:b2:d8:12:cb；仅写应用区0x20000，4433248字节，工具返回0、hash校验通过并执行复位，未写bootloader/分区/NVS或更新C6。随后12秒只读采集得到286字节，含背光设置和切换ESP-NOW返回ESP_OK，说明捕获到运行活动，但日志不构成屏幕布局目视验收。串口已关闭。日志tmp/wireless-eight-preflash-build.log、tmp/wireless-eight-flash.log、tmp/wireless-eight-startup.log。新布局与显示规则仍待用户实屏确认。

2026-11-13 无线串口视觉风格按确认效果图调整并烧录COM13：浅蓝垂直渐变底色，蓝色选中页签/主发送按钮，浅蓝关闭与浅绿开启状态卡，深蓝文字，五色重复细边框数据卡，放大标题/状态开关/主要按钮文字，底部分隔线与灰色关闭开关。状态标题与开/关分层显示，一行8个状态按钮；数值保持Consolas40等宽右对齐、普通十进制最多6字符、超出隐藏，不改变编辑值/48字节协议。新增20/28/36px无线页中文字形子集，并回退到原中文字库以覆盖编辑器与动态提示，不改全局字库。源码备份tmp/wireless-style-before-20261113。最终构建成功，无编译warning/error；应用4531984字节；esptool仅写COM13应用区0x20000，返回0，hash校验通过并复位。数值格式/等宽字宽/8按钮边界回归检查通过。日志tmp/wireless-style-build-final.log、tmp/wireless-style-flash.log、tmp/wireless-style-startup.log。尚未取得实屏截图进行逐项视觉验收，不能宣称与生成图逐像素一致。串口已关闭；未更新任何C6固件。

2026-11-13 无线发送页新增“全零发送”按钮，位于发送一次右侧x300/y478/w260/h56，沿用浅蓝蓝边次级按钮与28px字形（补齐全/零）。仅CLICKED事件且处于活动发送页时构造独立serial_frame_t，serial_frame_init后调用serial_transport_send一次，10项数据均0，帧头AAAA/帧尾EEEE保持有效；不修改当前编辑值、状态开关或原持续发送设置；接收/曲线页隐藏该按钮。备份tmp/wireless-zero-before-20261113。构建通过，无编译warning/error；主机协议测试包含新增40字节数据区全零断言及原10000帧噪声/分片解析用例，通过（TinyCC主机端单独使用strtof兼容shim，不涉及固件代码）。用户授权范围内更新COM13 P4，仅写应用区0x20000，应用4532992字节，工具返回0/hash校验通过并复位。未自动点击按钮发送无线数据，尚未验收真实对端收到全零帧。日志tmp/wireless-zero-build.log、tmp/wireless-zero-flash.log，测试tmp/wireless-zero-protocol-test.c。未修改C6。

2026-11-18 全零发送按用户要求同步清除面板编辑值：回调改为serial_frame_init(&s->tx)后仅发送一次&s->tx，并调用refresh刷新10项数字和8个请求状态。协议头尾保持，未清除接收历史指示点；原持续发送开关不变，若已开启后续沿用全零模型。即使发送拒绝也保留已清零的本地值，显示发送失败。备份tmp/wireless-zero-clear-before-20261118.c。构建通过，无编译warning/error。提取实际回调进行原生C桩测试：原编辑数据全清零、单次发送、面板刷新、失败清零及接收/曲线/编辑/非点击/非活动保护均通过。COM13识别ESP32-P4，只写应用区0x20000，4532992字节，hash校验通过并复位。未自动触发实物按钮发送，屏幕视觉及对端接收待现场确认。日志tmp/wireless-zero-clear-build.log、tmp/wireless-zero-clear-flash.log，测试tmp/wireless-zero-clear-test.c。未更新C6。

2026-11-18 按用户要求屏蔽无线串口曲线并排查驻留卡顿：移除无线页曲线入口、LVGL图表对象、12000点曲线缓存、每周期历史读取，以及无线传输层512点曲线history写入与分配；保留最新帧、协议/ACK/收发缓存。页面100ms轮询，按状态键/可见文本缓存仅变化时写LVGL样式与标签，字体/布局固定在创建时，直接由原始数值格式化，减少字符串往返。此前发送/接收页也一直collect历史且周期性重复设置样式，是确定的额外开销；曲线历史本身有界，不能据此认定原卡顿是无限泄漏或已证明唯一根因。备份tmp/wireless-noplot-before-20261118，说明WIRELESS_PERFORMANCE.md。
最终构建通过，无编译warning/error；应用4527296字节，COM13 ESP32-P4仅写0x20000应用区，返回0、hash校验通过并复位。未更新C6。实际refresh函数主机桩测试10万次不变数据轮询零LVGL写入，状态bit7、数值变化、接收状态和页面切换检查通过；曲线入口/图表/采集移除静态检查通过，六字符数值与全零模型回归通过。
烧录后约125秒真实页面驻留，交替发送/接收页，24条now stats样本；ui_ticks48→1226（5.06→123.33秒），刷新回调最大4682us，PSRAM空闲27626532字节不变；内部空闲首尾183907→183747，整个过程波动944字节；最大连续内部块139264字节不变。曲线启用false、history_points0。未发现本轮驻留持续内存下降，尚不能替代数小时带数据负载及触屏响应验证。当前COM10/外部串口数据源不可用，本轮没有持续无线数据压力测试，不宣称彻底解决长时间卡顿。串口已关闭，页面停在发送页。
证据tmp/wireless-noplot-build-final.log、tmp/wireless-noplot-flash.log、tmp/wireless-noplot-soak.json、tmp/wireless-noplot-soak.log、tmp/wireless-noplot-soak-result.log，测试tmp/wireless-refresh-test.c。
