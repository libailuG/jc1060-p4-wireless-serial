# TouchDesk 整套应用迁移

## 已确认范围

在 JC1060 P4 底座上逐项迁移，不覆盖 BSP，不重新压缩字体，不擦除用户数据。
保留天气时钟、有线串口/曲线、ESP-NOW 无线串口、USB 键鼠转 BLE、触屏 USB 快捷键、
计数器、设置、信息、小智。移除 HC-08、音乐与独立麦克风应用。
小智输入输出改用板载麦克风/ES8311/扬声器。
ESP-NOW 应用期间暂停天气联网更新，返回桌面后恢复；无线服务须统一管理频道。
接受更新板载 C6，优先 SDIO 扩展。用户已授权自主烧录 P4/C6；0.6.0 已通过 OTA 更新板载 C6 至 Hosted 2.11.7 并加入 ESP-NOW 转发。

## 来源

完整系统：2026-10-23 输出目录 TouchDeskWireless，0.9.3-hc08，包含 10 月 26 日稳定大时钟。
其 main 的 88 个文件与 touchdesk-hc08-wireless-uart-github/firmware/main 哈希一致，
对应提交 ca35ac4。ESP-NOW 增量单独取自 2026-10-28 输出，不能替代完整系统基线。

## 当前进度：0.6.0-dev（ESP-NOW 无线串口）

原 S3 1.3 无线 UI 已迁入 LVGL9，保留发送/接收/曲线布局和数据结构。P4 通过 SDIO 自定义 RPC 调用板载 C6 ESP-NOW，外接 C6 COM12 编程/状态、COM10 业务串口。固定频道6、原加密密钥与 ARQ 保留。应用退出恢复已保存 Wi-Fi，进入/退出切换通过后台服务重试，避免系统忙碌时漏掉清理。

升级使用 PSRAM 分块暂存、偏移应答重试、整镜像 SHA256 和芯片型号校验；不自动擦除 NVS。板载 C6 首次未验证则90秒回退，验证结果按镜像 SHA 持久化。P4 重启后外接 C6 可以重新握手，并重编号保留未确认的上行块。修复曲线删除事件访问已释放状态，以及逐点刷新造成的绘图开销。

详细使用、备份/回退和测试脚本见 [ESP_NOW.md](ESP_NOW.md)。实机测试结果继续记录于 VALIDATION.md。

## 历史进度：0.5.0-dev（USB 键鼠 → BLE）

已新增“键鼠蓝牙桥接”应用，迁入旧系统 mouse_report、mouse_flow、keyboard_flow、BLE HOGP 和 USB HID Host。
P4 通过 SDIO Hosted VHCI 使用板载 C6 BLE 控制器，不占 UART1，不更新 C6。
应用退出关闭转发并清空/释放按键；转发检查与撤销使用同一临界区，避免退出时最后一包穿透。
标准 BOOT 键盘、鼠标及 USB Hub 已配置；完整范围与使用方法见 USB_BLE_BRIDGE.md。
配对名称 TouchDesk KeyMouse，配对窗口120秒；配对数据保留，不自动清除 NVS 或 BLE bonds。

已通过：
- IDF6.1 + LVGL9.5 构建与COM13写入hash校验。
- 原键鼠 C 测试（键盘排序/修饰键/回绕队列/重连隔离，12位鼠标XY、滚轮、Report ID、截断和有符号分块）。
- Hosted 控制器生命周期 API 的2.5.2边界测试；旧版无需新RPC，实际HCI/广播仍须验证。
- P4实机：HOGP服务启动、USB Host HID启动ESP_OK、NimBLE开始广播TouchDesk KeyMouse。
- 桥接Host→USB快捷键Device→桥接Host切换均ESP_OK，接口/驱动清理与重启通过（未插实际键鼠）。
- 本次从端RPC版本查询返回2.3.2，旧握手日志曾显示2.3.0；均属于2.5.2以前，不推定C6被更新。
- 初次发现USB重复上电返回INVALID_STATE，已修复端口电源状态跟踪；新版控制器RPC在旧从端返回NOT_SUPPORTED，已按版本修复。

用户目前没有键鼠/OTG转接头，明确要求先完成固件；电脑配对、实际USB枚举、键鼠输入及长期共存尚未验收。
当前镜像6,347,392字节，SHA256：027D9D152EC6B2D6F0BFE1FC0A780FDB2CE517BCFA879FDE68AD1731FBE8A118。
构建/烧录及运行日志：tmp/ble-bridge-legacy-flash.log、ble-bridge-legacy-runtime.log。
## 历史进度：0.4.0-dev（USB 快捷键）

已加入 USB 快捷键应用：20 个常用组合键、鼠标左键/四向移动、音量加减/静音、释放全部。
USB 服务来自旧系统，适配 TinyUSB 2.3.0 事件与描述符配置，使用 P4 高速 OTG port 1；
设备唯一标识使用 P4 efuse。COM13 仍仅供调试，不能代替高速 OTG 键鼠连接。
应用退出撤销输入权限、清空队列并释放按键；驱动可以驻留，切换 USB Host 时先卸载 Device。
诊断 USB 枚举改为可停止任务与客户端生命周期，切换 Device 前等待 Host 设备释放和事件任务退出。
不使用原 BSP 直接删除仍运行 USB 任务的停止实现。卸载失败则拒绝切换角色。

验证：
- 新版构建通过并烧录 COM13，写入 hash 通过。镜像6,147,328字节，SHA256：91D9C8FCF6C37E5D57BB73ED0D09800CEDF6391A63E1F84A5DAB04C24DDECAF4。
- tests/usb/touch_hid.py：生产 worker 的按键保持、满队列释放、鼠标分块、媒体键释放重试、
  退出撤销、旧会话拒绝、断线不重放、Host 占用及 Device 卸载失败测试均通过。
- 实机两轮 HID start/stop 与 USB Host→Device 切换均 ESP_OK，TinyUSB installed on port1。
- 未连接电脑验证真实 HID 枚举/按键，未自动注入按键。触摸键盘和完整触控板界面尚未迁完。
- 字库 SHA256 与原未压缩版一致；分区、NVS 和 C6 固件未变。
- 已确认扬声器需要外接；音频排查搁置，不作为本轮任务。

下一步：USB 键鼠→Hosted BLE 桥接，以及 C6 ESP-NOW 扩展。旧握手日志 capabilities=0xd，
显示 C6 支持 BLE、Host BT Disabled；需开启并验证 P4 Hosted HCI，不能只凭能力位宣称桥接通过。
自定义 Peer Data 接口在本地 Hosted changelog 2.8.1/2.8.2 才新增/调整，
板上2.3.0不能直接按2.11.7示例假定兼容，ESP-NOW仍需匹配从端固件方案。
日志：tmp/usb-shortcuts-build-final.log、usb-shortcuts-flash.log、usb-shortcuts-runtime.log。
## 历史进度：0.3.0-dev

本批移除底部常驻日志区，应用区高度 482px。网络和设置只显示简短操作状态，详细记录在诊断页。
已增加天气/日期/农历桌面摘要、城市天气和四日预报、SNTP、有线 RS485 结构体收发、
20ms 持续发送、512 样本历史、八通道曲线选择和自动范围。
小智已迁入原会话/激活/WebSocket/MQTT/Opus 逻辑，音频后端改为板载 48kHz 单声道 PCM，
沿用原重采样到 16kHz Opus；诊断音频与语音业务采用互斥占用。
不包含音乐、HC-08、独立麦克风应用。

已完成的验证：
- 天气/串口/小智及音频后端构建通过；使用 IDF6 独立 cJSON/MQTT 组件与原版本 WebSocket/音频编解码组件。
- 原串口帧、历史、曲线 C 测试均通过；主机浮点兼容层使用旧测试同样的 strtod→float 适配。
- 新增 tests/voice_backend_test.py，通过音频格式、分块、模式切换、占用互斥及打开失败释放测试。
- 首次实机检查：Wi-Fi 扫描成功、提示音和两秒录音回放 ESP_OK。PCM 峰值满幅，录音质量尚未验收。
- 首次小智初始化返回 ESP_ERR_NOT_SUPPORTED，追踪为 P4 不支持 ESP_MAC_WIFI_STA 标识读取，
  已改用 P4 出厂 efuse MAC，仍保留独立 NVS UUID；修复版实机初始化 ESP_OK、available=1。
- 修复版 Wi-Fi 连接取得 IP，天气 HTTP200、解析 ESP_OK。

尚未完成：ESP-NOW 板载 C6 扩展、USB 触屏快捷键、USB 键鼠转 BLE、完整天气外观/主题与资源迁移、
串口旧界面的全部交互，以及各应用的完整实机验收。当前不是“全部应用完成版”。
有线接口使用板上 RS485，不能直接连接 TTL 串口。C6 2.3.0 / Host 2.11.7 告警仍未解决。
## 第一批源码

- 应用注册集中到 main/app_registry.c；旧 boot_sample 仍指向原示例。
- 首页应用入口支持三列、多行滚动。
- 独立时钟页显示 UTC+8；系统时间无效时显示未校时。尚未接 SNTP、RTC 或天气。
- 计数器支持加减和手动保存，使用独立 counter_p4/value，不改变 system/config_v1。
- services/serial_frame、serial_history、serial_plot 从完整基线原样迁入，
  保留 48 字节小端帧、字段校验、512 样本历史与时间窗口曲线算法。
- 原 C 测试放在 tests/serial。此批尚未接入串口 UI、UART 或 ESP-NOW 传输。

本批只是迁移起点，不代表整套应用完成或实机通过。诊断中的录音回放保留为板载音频验收入口，
不作为独立麦克风应用注册。

## 后续关卡

1. 实现统一的应用服务生命周期、后台任务和资源互斥。
2. 串口 UI 与物理接口适配（TTL/RS485 必须按实际对端区分）。
3. 天气、SNTP 与 RTC 对接，确保扫描、联网和 ESP-NOW 频道策略互斥。
4. 固定 Host/C6 配套版本；验证 SDIO 自定义数据通道后扩展 C6 ESP-NOW。
5. Hosted BLE HID、USB HID Host/Device 分别验证后迁移键鼠应用。
6. 板载音频持续采集/播放后端，再迁移小智会话和编解码。
7. 全应用切换、积压排空、断网恢复、断电保留与长时间稳定性验收。

不要将独立 ESP-NOW 实验的 1000/2500 帧成绩记为整套应用通过。
板载 C6 现有 Hosted 2.3.0 与 Host 2.11.7 的版本差异仍未解决。

## 回退

本批修改前 main、PLATFORM.md、VALIDATION.md 和 build.ps1 位于工作区
tmp/migration-start-20261030。未改变分区、BSP、字体源、生成脚本或 C6 源码。
第一批随后经用户授权烧录到 COM13，写入 hash 校验通过；设备已被工具复位，运行行为尚待验收。build 目录为新镜像，不能作为原版回退镜像。






