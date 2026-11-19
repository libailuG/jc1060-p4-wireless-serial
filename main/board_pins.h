#pragma once
/* JC1060P470C_I_W_Y V3 原理图。不要照抄 ESP32-P4 官方评估板的 USB 引脚宏。 */
#define BOARD_BOOT_GPIO       35 /* 同时是 RMII TXD1：Ethernet 工作时不能用作按键 */
#define BOARD_RS485_TX        26
#define BOARD_RS485_RX        27
#define BOARD_ETH_MDC         31
#define BOARD_ETH_MDIO        52
#define BOARD_ETH_RESET       51
#define BOARD_ETH_PHY_ADDR    1
#define BOARD_RTC_ADDR        0x32
/* I2C SDA7/SCL8; LCD RST0/BL23; touch INT21/RST22 (BSP uses polling).
 * Audio BCLK12/MCLK13/WS10/DOUT9/DIN48/PA11.
 * TF slot0 D0..3=39..42, CLK43, CMD44.
 * C6 SDIO slot1 CLK18/CMD19/D0..3=14..17/RESET54.
 * USB Serial/JTAG 是 COM13；USB OTG 使用 P4 专用高速 PHY。 */
