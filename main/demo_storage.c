#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "demo.h"
#include "bsp/esp-bsp.h"
#include "sd_pwr_ctrl_by_on_chip_ldo.h"
#include "nvs.h"
#include "esp_check.h"

static sdmmc_card_t *card;
static sd_pwr_ctrl_handle_t sd_power;

static esp_err_t mount_sd(void)
{
    if (card) return ESP_OK;
    if (!sd_power) {
        sd_pwr_ctrl_ldo_config_t power = {.ldo_chan_id = 4};
        ESP_RETURN_ON_ERROR(sd_pwr_ctrl_new_on_chip_ldo(&power, &sd_power), "sd", "power");
    }
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    host.slot = SDMMC_HOST_SLOT_0; /* C6 使用 slot1，两者不能互换 */
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;
    host.pwr_ctrl_handle = sd_power;
    sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
    slot.width = 4;
    esp_vfs_fat_sdmmc_mount_config_t mount = {
        .format_if_mount_failed = false, .max_files = 5, .allocation_unit_size = 16384,
    };
    /* 挂载失败不会格式化 TF 卡；可插卡后再次点击。 */
    return esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot, &mount, &card);
}

esp_err_t demo_storage_test(char *result, size_t capacity)
{
    esp_err_t ret = mount_sd();
    if (ret != ESP_OK) {
        snprintf(result, capacity, "Insert a FAT-formatted TF card. No formatting was performed.");
        return ret;
    }
    /* mkstemp 避免覆盖卡上已有文件。只删除本次创建的临时文件。 */
    char path[] = "/sdcard/jc1060_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) return ESP_FAIL;
    const char payload[] = "JC1060: SDMMC write/read example\n";
    char readback[sizeof(payload)] = {0};
    bool ok = write(fd, payload, sizeof(payload)) == sizeof(payload);
    if (ok) ok = fsync(fd) == 0 && lseek(fd, 0, SEEK_SET) == 0;
    if (ok) ok = read(fd, readback, sizeof(readback)) == sizeof(readback);
    if (close(fd) != 0) ok = false;
    if (ok) ok = memcmp(payload, readback, sizeof(payload)) == 0;
    bool removed = unlink(path) == 0;
    snprintf(result, capacity, "Card %.8s: %llu MB. Readback %s; temporary file %s.",
        card->cid.name, (unsigned long long)card->csd.capacity * card->csd.sector_size / (1024 * 1024),
        ok ? "matched" : "FAILED", removed ? "removed" : path);
    return ok && removed ? ESP_OK : ESP_FAIL;
}

esp_err_t demo_nvs_test(char *result, size_t capacity)
{
    nvs_handle_t nvs;
    ESP_RETURN_ON_ERROR(nvs_open("jc1060_demo", NVS_READWRITE, &nvs), "nvs", "open");
    uint32_t counter = 0;
    esp_err_t ret = nvs_get_u32(nvs, "clicks", &counter);
    if (ret == ESP_ERR_NVS_NOT_FOUND) ret = ESP_OK;
    if (ret == ESP_OK) ret = nvs_set_u32(nvs, "clicks", ++counter);
    if (ret == ESP_OK) ret = nvs_commit(nvs);
    uint32_t readback = 0;
    if (ret == ESP_OK) ret = nvs_get_u32(nvs, "clicks", &readback);
    nvs_close(nvs);
    if (ret == ESP_OK && readback != counter) ret = ESP_FAIL;
    snprintf(result, capacity, "Persistent counter: %lu (namespace jc1060_demo, key clicks)", (unsigned long)readback);
    return ret;
}
