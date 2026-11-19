#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include "platform.h"
#include "demo_text.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static SemaphoreHandle_t guard;
static QueueHandle_t jobs;
static atomic_bool busy;
static platform_config_t config;
static char latest[512] = "系统就绪";
static char history[8][160];
static unsigned history_next, history_count;
typedef struct { char title[64]; demo_action_fn run; } job_t;

static platform_config_t defaults(void)
{
    return (platform_config_t){.brightness = 75, .device_name = "JC1060", .idle_seconds = 0};
}

void platform_config_get(platform_config_t *out)
{
    xSemaphoreTake(guard, portMAX_DELAY);
    *out = config;
    xSemaphoreGive(guard);
}

esp_err_t platform_config_set(const platform_config_t *value)
{
    if (value->brightness < 10 || value->brightness > 100 || value->idle_seconds > 3600 ||
        !memchr(value->ssid, 0, sizeof(value->ssid)) ||
        !memchr(value->password, 0, sizeof(value->password)) ||
        !memchr(value->device_name, 0, sizeof(value->device_name))) return ESP_ERR_INVALID_ARG;
    xSemaphoreTake(guard, portMAX_DELAY);
    config = *value;
    xSemaphoreGive(guard);
    return ESP_OK;
}

esp_err_t platform_config_save(char *result, size_t capacity)
{
    platform_config_t value;
    platform_config_get(&value);
    nvs_handle_t handle;
    esp_err_t ret = nvs_open("system", NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_set_blob(handle, "config_v1", &value, sizeof(value));
        if (ret == ESP_OK) ret = nvs_commit(handle);
        nvs_close(handle);
    }
    snprintf(result, capacity, "%s", ret == ESP_OK ? "设置已保存，重启后保留。" : "保存失败；当前设置仅在本次运行生效。");
    memset(value.password, 0, sizeof(value.password));
    return ret;
}

esp_err_t platform_config_reset(char *result, size_t capacity)
{
    platform_config_t value = defaults();
    platform_config_set(&value);
    return platform_config_save(result, capacity);
}

void demo_post_status(const char *text)
{
    ESP_LOGI("system", "%s", text);
    if (!guard) return;
    xSemaphoreTake(guard, portMAX_DELAY);
    demo_text_copy(latest, sizeof(latest), text, strlen(text), true);
    char first[120];
    demo_text_copy(first, sizeof(first), text, strcspn(text, "\n"), false);
    snprintf(history[history_next], sizeof(history[0]), "[%llds] %s",
             (long long)(esp_timer_get_time() / 1000000), first);
    history_next = (history_next + 1) % 8;
    if (history_count < 8) history_count++;
    xSemaphoreGive(guard);
}

void platform_latest(char *out, size_t capacity)
{
    xSemaphoreTake(guard, portMAX_DELAY);
    demo_text_copy(out, capacity, latest, strlen(latest), true);
    xSemaphoreGive(guard);
}

void platform_history(char *out, size_t capacity)
{
    size_t used = 0;
    if (!capacity) return;
    out[0] = 0;
    xSemaphoreTake(guard, portMAX_DELAY);
    for (unsigned i = 0; i < history_count; i++) {
        const char *line = history[(history_next + 8 - history_count + i) % 8];
        size_t len = strlen(line);
        if (used + len + 2 > capacity) break;
        memcpy(out + used, line, len);
        used += len;
        out[used++] = '\n'; out[used] = 0;
    }
    xSemaphoreGive(guard);
}

static void worker(void *arg)
{
    job_t job;
    while (xQueueReceive(jobs, &job, portMAX_DELAY)) {
        char result[512] = {0}, message[640];
        snprintf(message, sizeof(message), "正在执行：%s", job.title);
        demo_post_status(message);
        esp_err_t ret = job.run(result, sizeof(result));
        snprintf(message, sizeof(message), "%s: %s\n%s", job.title, esp_err_to_name(ret), result);
        demo_post_status(message);
        atomic_store(&busy, false);
    }
}

esp_err_t platform_submit(const char *title, demo_action_fn run)
{
    if (!jobs || !run || !title) return ESP_ERR_INVALID_STATE;
    bool expected = false;
    if (!atomic_compare_exchange_strong(&busy, &expected, true)) return ESP_ERR_INVALID_STATE;
    job_t job = {.run = run};
    demo_text_copy(job.title, sizeof(job.title), title, strlen(title), false);
    if (xQueueSend(jobs, &job, 0) != pdTRUE) {
        atomic_store(&busy, false);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
bool platform_busy(void) { return atomic_load(&busy); }
esp_err_t demo_request_wifi_scan(void) { return platform_submit("Wi-Fi 扫描", demo_wifi_scan); }

esp_err_t platform_init(void)
{
    guard = xSemaphoreCreateMutex();
    jobs = xQueueCreate(1, sizeof(job_t));
    if (!guard || !jobs) return ESP_ERR_NO_MEM;
    config = defaults();
    nvs_handle_t handle;
    esp_err_t ret = nvs_open("system", NVS_READONLY, &handle);
    if (ret == ESP_OK) {
        platform_config_t saved;
        size_t size = sizeof(saved);
        ret = nvs_get_blob(handle, "config_v1", &saved, &size);
        nvs_close(handle);
        if (ret == ESP_OK) ret = size == sizeof(saved) ? platform_config_set(&saved) : ESP_ERR_INVALID_SIZE;
    }
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND)
        demo_post_status("配置未能读取，使用默认设置；未擦除 NVS。");
    if (xTaskCreate(worker, "system_worker", 12288, NULL, 5, NULL) != pdPASS) return ESP_ERR_NO_MEM;
    return ESP_OK;
}
