#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include "driver/gpio.h"
#include "demo.h"
#include "platform.h"
#include "demo_text.h"
#include "board_pins.h"
#include "esp_check.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "esp_eth_phy_ip101.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

static EventGroupHandle_t wifi_events;
static bool wifi_ready;
static atomic_bool wifi_connecting;
static atomic_uint retry;
static esp_netif_t *wifi_netif;
#define GOT_IP BIT0
#define FAILED BIT1
#define SCAN_DONE BIT2
#define DISCONNECTED BIT3
static atomic_int scan_status;
static portMUX_TYPE ap_guard = portMUX_INITIALIZER_UNLOCKED;
static char ap_names[8][33];
static unsigned ap_count;
unsigned demo_wifi_ap_snapshot(char names[8][33])
{
    portENTER_CRITICAL(&ap_guard);
    memcpy(names, ap_names, sizeof(ap_names));
    unsigned count = ap_count;
    portEXIT_CRITICAL(&ap_guard);
    return count;
}

esp_err_t demo_network_init(void)
{
    ESP_RETURN_ON_ERROR(esp_netif_init(), "net", "netif");
    esp_err_t ret = esp_event_loop_create_default();
    return ret == ESP_ERR_INVALID_STATE ? ESP_OK : ret;
}
static void wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = data;
        char text[100];
        snprintf(text, sizeof(text), "Wi-Fi got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        demo_post_status(text);
        retry = 0;
        xEventGroupSetBits(wifi_events, GOT_IP);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_SCAN_DONE) {
        wifi_event_sta_scan_done_t *event = data;
        atomic_store(&scan_status, event->status);
        xEventGroupSetBits(wifi_events, SCAN_DONE);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(wifi_events, GOT_IP);
        xEventGroupSetBits(wifi_events, DISCONNECTED);
        if (wifi_connecting && retry++ < 3) {
            if (esp_wifi_connect() != ESP_OK) xEventGroupSetBits(wifi_events, FAILED);
        } else if (wifi_connecting) {
            wifi_connecting = false;
            xEventGroupSetBits(wifi_events, FAILED);
            demo_post_status("Wi-Fi disconnected. Check credentials, then select Connect again.");
        }
    }
}
static esp_err_t wifi_init(void)
{
    if (wifi_ready) return ESP_OK;
    demo_post_status("Wi-Fi: initializing P4-C6 SDIO link...");
    if (!wifi_events) wifi_events = xEventGroupCreate();
    if (!wifi_events) return ESP_ERR_NO_MEM;
    if (!wifi_netif) wifi_netif = esp_netif_create_default_wifi_sta();
    if (!wifi_netif) return ESP_ERR_NO_MEM;
    /* esp_wifi_* 在 P4 上由 esp_wifi_remote/esp_hosted 转发给 C6。
     * 此处不需要写 SDIO 传输协议，也不烧写 C6。 */
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&config), "wifi", "remote init");
    demo_post_status("Wi-Fi: C6 initialized; starting station...");
    esp_err_t ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event, NULL);
    if (ret == ESP_OK) ret = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event, NULL);
    if (ret == ESP_OK) ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (ret == ESP_OK) ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret == ESP_OK) ret = esp_wifi_start();
    if (ret != ESP_OK) {
        esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event);
        esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event);
        esp_wifi_deinit();
        return ret;
    }
    wifi_ready = true;
    ESP_LOGI("wifi", "Station ready");
    return ESP_OK;
}
esp_err_t demo_wifi_scan(char *result, size_t capacity)
{
    esp_err_t ret = wifi_init();
    if (ret != ESP_OK) {
        snprintf(result, capacity, "C6 initialization failed: %s. See SDIO/RPC messages in serial log.", esp_err_to_name(ret));
        return ret;
    }
    demo_post_status("Wi-Fi: scanning nearby networks (timeout 15 s)...");
    xEventGroupClearBits(wifi_events, SCAN_DONE);
    wifi_scan_config_t config = {.show_hidden = false};
    ESP_RETURN_ON_ERROR(esp_wifi_scan_start(&config, false), "wifi", "scan");
    EventBits_t bits = xEventGroupWaitBits(wifi_events, SCAN_DONE, pdTRUE, pdFALSE, pdMS_TO_TICKS(15000));
    if (!(bits & SCAN_DONE)) {
        esp_wifi_scan_stop();
        snprintf(result, capacity, "No scan-complete event within 15 s. Check C6 firmware and SDIO link.");
        return ESP_ERR_TIMEOUT;
    }
    if (atomic_load(&scan_status)) {
        snprintf(result, capacity, "Scan completion status: %d", atomic_load(&scan_status));
        return ESP_FAIL;
    }
    wifi_ap_record_t aps[8];
    uint16_t count = 8;
    ESP_RETURN_ON_ERROR(esp_wifi_scan_get_ap_records(&count, aps), "wifi", "results");
    portENTER_CRITICAL(&ap_guard);
    memset(ap_names, 0, sizeof(ap_names));
    for (unsigned i = 0; i < count; i++) memcpy(ap_names[i], aps[i].ssid, 32);
    ap_count = count;
    portEXIT_CRITICAL(&ap_guard);
    size_t used = snprintf(result, capacity, "%u access points (first 8):", count);
    for (unsigned i = 0; i < count && used < capacity; i++) {
        char ssid[129], line[160];
        demo_text_copy(ssid, sizeof(ssid), aps[i].ssid, 32, false);
        snprintf(line, sizeof(line), "\n%s  %d dBm", ssid, aps[i].rssi);
        /* 一整行放不下就停止，避免把 UTF-8 字符截成半个。 */
        size_t len = strlen(line);
        if (len >= capacity - used) break;
        memcpy(result + used, line, len + 1);
        used += len;
    }
    return ESP_OK;
}
esp_err_t demo_wifi_connect(char *result, size_t capacity)
{
    platform_config_t saved;
    platform_config_get(&saved);
    if (!saved.ssid[0]) {
        snprintf(result, capacity, "请在网络页面填写 SSID 和密码，选择保存并连接。");
        return ESP_ERR_INVALID_STATE;
    }
    ESP_RETURN_ON_ERROR(wifi_init(), "wifi", "init");
    /* Stop previous session before changing credentials. Wait for its event so
     * a late disconnect cannot trigger a retry against the new configuration. */
    bool had_session = wifi_connecting || (xEventGroupGetBits(wifi_events) & GOT_IP);
    wifi_connecting = false;
    if (had_session) {
        xEventGroupClearBits(wifi_events, DISCONNECTED);
        esp_err_t disconnect_ret = esp_wifi_disconnect();
        if (disconnect_ret != ESP_OK) return disconnect_ret;
        if (!(xEventGroupWaitBits(wifi_events, DISCONNECTED,
            pdTRUE, pdFALSE, pdMS_TO_TICKS(3000)) & DISCONNECTED)) {
            snprintf(result, capacity, "等待旧连接断开超时，请重试。");
            return ESP_ERR_TIMEOUT;
        }
    }
    wifi_config_t config = {0};
    memcpy(config.sta.ssid, saved.ssid, strlen(saved.ssid));
    memcpy(config.sta.password, saved.password, strlen(saved.password));
    memset(saved.password, 0, sizeof(saved.password));
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &config), "wifi", "config");
    xEventGroupClearBits(wifi_events, GOT_IP | FAILED);
    retry = 0;
    wifi_connecting = true;
    esp_err_t ret = esp_wifi_connect();
    if (ret != ESP_OK) { wifi_connecting = false; return ret; }
    EventBits_t bits = xEventGroupWaitBits(wifi_events, GOT_IP | FAILED, pdFALSE, pdFALSE, pdMS_TO_TICKS(30000));
    if (!(bits & GOT_IP)) {
        wifi_connecting = false;
        esp_wifi_disconnect();
        snprintf(result, capacity, "连接失败或 30 秒内未取得 IP，请检查凭据、路由器和 C6 兼容性。");
        return bits & FAILED ? ESP_FAIL : ESP_ERR_TIMEOUT;
    }
    esp_netif_ip_info_t ip;
    ESP_RETURN_ON_ERROR(esp_netif_get_ip_info(wifi_netif, &ip), "wifi", "IP");
    snprintf(result, capacity, "Connected: " IPSTR ". TCP/HTTP/MQTT clients can now use lwIP sockets.", IP2STR(&ip.ip));
    return ESP_OK;
}

void demo_wifi_status(char *out, size_t capacity)
{
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    esp_netif_ip_info_t ip = {0};
    if (netif && esp_netif_is_netif_up(netif) && esp_netif_get_ip_info(netif, &ip) == ESP_OK && ip.ip.addr)
        snprintf(out, capacity, IPSTR, IP2STR(&ip.ip));
    else snprintf(out, capacity, "未连接");
}

static esp_eth_handle_t ethernet;
static esp_netif_t *eth_netif;
static esp_eth_netif_glue_handle_t eth_glue;
static void eth_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == IP_EVENT) {
        const ip_event_got_ip_t *event = data;
        char text[100];
        snprintf(text, sizeof(text), "Ethernet got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        demo_post_status(text);
    } else if (id == ETHERNET_EVENT_CONNECTED) demo_post_status("Ethernet link up; waiting for DHCP.");
    else if (id == ETHERNET_EVENT_DISCONNECTED) demo_post_status("Ethernet link down.");
}
esp_err_t demo_ethernet_start(char *result, size_t capacity)
{
    if (ethernet) {
        esp_netif_ip_info_t ip;
        ESP_RETURN_ON_ERROR(esp_netif_get_ip_info(eth_netif, &ip), "eth", "IP");
        snprintf(result, capacity, "Ethernet already started. IP: " IPSTR ". Do NOT press BOOT (shared TXD1 GPIO35).", IP2STR(&ip.ip));
        return ESP_OK;
    }
    if (gpio_get_level(BOARD_BOOT_GPIO) == 0) {
        snprintf(result, capacity, "Release BOOT before starting Ethernet: GPIO35 is also RMII TXD1.");
        return ESP_ERR_INVALID_STATE;
    }
    demo_button_pause();
    eth_mac_config_t mac_config = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config = ETH_PHY_DEFAULT_CONFIG();
    phy_config.phy_addr = BOARD_ETH_PHY_ADDR;
    phy_config.reset_gpio_num = BOARD_ETH_RESET;
    eth_esp32_emac_config_t emac = ETH_ESP32_EMAC_DEFAULT_CONFIG();
    emac.smi_gpio.mdc_num = BOARD_ETH_MDC;
    emac.smi_gpio.mdio_num = BOARD_ETH_MDIO;
    emac.dma_burst_len = ETH_DMA_BURST_LEN_4;
    esp_eth_mac_t *mac = esp_eth_mac_new_esp32(&emac, &mac_config);
    esp_eth_phy_t *phy = esp_eth_phy_new_ip101(&phy_config);
    esp_err_t ret = ESP_ERR_NO_MEM;
    if (!mac || !phy) goto fail;
    esp_eth_config_t config = ETH_DEFAULT_CONFIG(mac, phy);
    ret = esp_eth_driver_install(&config, &ethernet);
    if (ret != ESP_OK) goto fail;
    esp_netif_config_t netif_config = ESP_NETIF_DEFAULT_ETH();
    eth_netif = esp_netif_new(&netif_config);
    eth_glue = esp_eth_new_netif_glue(ethernet);
    if (!eth_netif || !eth_glue) { ret = ESP_ERR_NO_MEM; goto fail; }
    ret = esp_netif_attach(eth_netif, eth_glue);
    if (ret == ESP_OK) ret = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event, NULL);
    if (ret == ESP_OK) ret = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_event, NULL);
    if (ret == ESP_OK) ret = esp_eth_start(ethernet);
    if (ret == ESP_OK) {
        snprintf(result, capacity, "IP101 started. Connect RJ45 cable for DHCP. BOOT key disabled: do NOT press it while Ethernet is active (shared TXD1 GPIO35).");
        return ret;
    }
fail:
    esp_event_handler_unregister(ETH_EVENT, ESP_EVENT_ANY_ID, eth_event);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_ETH_GOT_IP, eth_event);
    if (eth_glue) { esp_eth_del_netif_glue(eth_glue); eth_glue = NULL; }
    if (eth_netif) { esp_netif_destroy(eth_netif); eth_netif = NULL; }
    if (ethernet) { esp_eth_driver_uninstall(ethernet); ethernet = NULL; }
    if (mac) mac->del(mac);
    if (phy) phy->del(phy);
    demo_button_start();
    return ret;
}

bool demo_wifi_connected(void)
{
    return wifi_events && (xEventGroupGetBits(wifi_events) & GOT_IP);
}

esp_err_t demo_wifi_wireless_mode(bool enabled)
{
    if(enabled) {
        esp_err_t e=wifi_init();if(e!=ESP_OK)return e;
        wifi_connecting=false;
        bool connected=(xEventGroupGetBits(wifi_events)&GOT_IP)!=0;
        xEventGroupClearBits(wifi_events,DISCONNECTED);
        e=esp_wifi_disconnect();
        if(e!=ESP_OK&&e!=ESP_ERR_WIFI_NOT_CONNECT)return e;
        if(connected&&!(xEventGroupWaitBits(wifi_events,DISCONNECTED,pdTRUE,pdFALSE,pdMS_TO_TICKS(3000))&DISCONNECTED))return ESP_ERR_TIMEOUT;
        xEventGroupClearBits(wifi_events,GOT_IP);
        return ESP_OK;
    }
    platform_config_t saved;platform_config_get(&saved);
    if(saved.ssid[0]){char result[128];return demo_wifi_connect(result,sizeof(result));}
    return ESP_OK;
}

