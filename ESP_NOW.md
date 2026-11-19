# ESP-NOW 无线串口

## 接口与界面

- P4 调试/烧录：COM13；外接 C6 烧录/状态：COM12；业务 USB-UART：COM10。
- 业务串口：921600、8N1、无硬件流控。外接 C6 UART1 TX=GPIO18、RX=GPIO19，交叉接线并共地，3.3V 逻辑。
- 通信双方：板载 C6 `7c:0c:5f:d9:31:14`，外接 C6 `20:6e:f1:0b:82:68`，固定频道 6，加密单播。
- 首页打开“ESP-NOW 无线串口”。复用原 S3 1.3 的发送/接收/曲线、10 项数据、4 个状态位、数字键盘、20ms 连发与曲线时间窗，适配 LVGL9 和 P4 屏幕比例。
- 无线应用占用 Wi-Fi 频道；进入时断开路由器，退出后连接已保存的 Wi-Fi。页面没有额外的底部日志区域。
- 数据帧沿用原 S3：48 字节、小端 `<III8fI>`，头 `0xAAAAAAAA`，两个 uint32、八个 float32、尾 `0xEEEEEEEE`。无线载荷按字节流传输；屏幕解析这些结构体。

## 实现位置

- `main/wireless/legacy_serial.c`：原 S3 页面迁移；`app_wireless.c`：应用生命周期和缩放。
- `main/wireless/transport.c`：原 TDR1 CRC32C、ACK/SACK、16 包窗口、120ms 重传；接收重排与屏幕历史存放 PSRAM。
- `dev/c6_espnow_hosted/slave`（相对工作区根目录）：板载 C6 使用 Hosted 2.11.7，保留 SDIO Wi-Fi / BLE，并增加 ESP-NOW 自定义 RPC 转发。不可向它烧录外接 C6 的独立固件。
- `dev/c6_uart_peer`：外接 C6 业务串口与 ESP-NOW 桥接。P4 接收 epoch 改变时重新握手，重编号并保留尚未确认的上行块。跨重启不保证业务层恰好一次，应用若有此需求应携带序号。
- 两端 `pairing.h` 保留原密钥，不在日志中输出，不应公开分享。

## 构建、升级与回退

P4 使用现有 `build.ps1 build` / `build.ps1 flash COM13`。外接 C6 只向 COM12 的 `0x10000` 写入 `touchdesk_c6_raw.bin`，保留原分区和 NVS。

板载 C6 通过 P4 的 `tools/stage-c6.py <network_adapter.bin> --log <日志路径> --ota` 更新；先在 PSRAM 暂存，64 字节带偏移的十六进制块应答/重试，整镜像 SHA256、镜像头与芯片型号验证通过后才允许 OTA。升级后工具自动证明自定义 RPC 通道。首次未获确认的镜像 90 秒后回退；确认结果以镜像 ELF SHA 保存 NVS，正常后续重启不再触发这个首次升级保护。

原始回退材料位于工作区 `backups/espnow-migration`：P4 更新前应用、板载 C6 原厂 2.3.2 镜像及提取的 app、COM12 成功读取的前 2MiB。失败的 16MiB 读取不能当作备份使用。恢复原厂板载 C6 时应先通过当前 P4 完成其 OTA，再恢复旧 P4 应用；不擦除 NVS/存储。第二次 OTA 会轮换覆盖槽位，原厂备份须保留在电脑上。

## 验证入口

`tools/test-espnow.py` 使用 COM13/12/10 检查 1000 帧上行、整段 CRC32C、末帧内容、曲线历史、20 帧真实 UI 发送到业务 UART 的逐字节一致性，以及页面重开和 Wi-Fi/BLE 回归。

串口 921600 是接口波特率，不代表无线链路能持续承载同等吞吐。缓冲区有限且没有 RTS/CTS，持续超速输入会丢弃并在 COM12 状态的 `dropped_bytes` / `uart_errors` 显示。主机应按实际链路能力控制发送速率。

## 2026-11-02 当前设备验证
已确认COM13/COM12/COM10双向链路可用，无需重复更新固件。1000帧上行CRC32C与20帧下行逐字节校验通过，重开页面3次通过。测试以每10ms发送一个48字节帧进行，不代表921600波特率满速无线吞吐。重开页面只回填最近512帧历史，history_lost可反映历史缓存覆盖。详见VALIDATION.md当日记录。
