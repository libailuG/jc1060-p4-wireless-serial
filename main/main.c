#include "demo.h"
#include "platform.h"
#include "esp_check.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "weather.h"

void app_main(void)
{
    /* 不自动 nvs_flash_erase：保留用户数据，故障时仅关闭相关功能。 */
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGW("main", "NVS unavailable: %s; no automatic erase", esp_err_to_name(ret));
    }
    ESP_ERROR_CHECK(platform_init());
    ret = demo_network_init();
    if (ret != ESP_OK) demo_post_status("网络基础服务不可用，请查看串口日志。");
    ret = weather_start();
    if (ret != ESP_OK) demo_post_status("天气后台服务启动失败。");
    demo_ui_start();
    demo_button_start();
    demo_console_start();
    ESP_LOGI("main", "JC1060 platform " PLATFORM_VERSION " ready");
}
