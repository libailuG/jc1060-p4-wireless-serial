#include <stdio.h>
#include <stdatomic.h>
#include "demo.h"
#include "usb_service.h"
#include "keymouse_service.h"
#include "usb/usb_host.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static usb_host_client_handle_t client;
static usb_device_handle_t devices[128];
static TaskHandle_t lib_task, enum_task;
static bool host_ready, root_on;
static atomic_bool stop_enum, stop_lib, all_free;
static void usb_event(const usb_host_client_event_msg_t *event, void *arg)
{
    char text[100];
    if (event->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        unsigned address = event->new_dev.address;
        if (address >= 128 || devices[address]) return;
        if (usb_host_device_open(client, address, &devices[address]) != ESP_OK) return;
        const usb_device_desc_t *d;
        if (usb_host_get_device_descriptor(devices[address], &d) == ESP_OK) {
            snprintf(text, sizeof(text), "USB VID=%04x PID=%04x class=%02x", d->idVendor, d->idProduct, d->bDeviceClass);
            demo_post_status(text);
        }
    } else if (event->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        for (unsigned i=1;i<128;i++) if (devices[i]==event->dev_gone.dev_hdl) {
            usb_host_device_close(client, devices[i]); devices[i]=NULL;
        }
    }
}
static void library(void *unused)
{
    for (;;) {
        if (atomic_load(&stop_lib)) { for (;;) vTaskSuspend(NULL); }
        uint32_t flags=0;
        usb_host_lib_handle_events(pdMS_TO_TICKS(50), &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) usb_host_device_free_all();
        if (flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) atomic_store(&all_free, true);
    }
}
static void enumerate(void *unused)
{
    while (!atomic_load(&stop_enum)) usb_host_client_handle_events(client, pdMS_TO_TICKS(50));
    for (unsigned i=1;i<128;i++) if (devices[i]) {
        usb_host_device_close(client, devices[i]); devices[i]=NULL;
    }
    for (;;) vTaskSuspend(NULL);
}
static bool await_suspend(TaskHandle_t task)
{
    for (unsigned i=0;i<100;i++) {
        if (eTaskGetState(task)==eSuspended) return true;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return false;
}
esp_err_t demo_usb_stop(void)
{
    if (!host_ready) return ESP_OK;
    keymouse_forward(false);
    esp_err_t bridge_error=keymouse_host_prepare_stop();
    if(bridge_error!=ESP_OK)return bridge_error;
    if(root_on) {
        bridge_error=usb_host_lib_set_root_port_power(false);
        if(bridge_error!=ESP_OK)return bridge_error;
        root_on=false;
    }
    bridge_error=keymouse_host_stop();
    if(bridge_error!=ESP_OK)return bridge_error;
    if (enum_task) {
        atomic_store(&stop_enum, true);
        if (!await_suspend(enum_task)) return ESP_ERR_TIMEOUT;
        esp_err_t e=usb_host_client_deregister(client);
        if (e!=ESP_OK) return e;
        client=NULL; vTaskDelete(enum_task); enum_task=NULL;
    }
    atomic_store(&all_free,false);
    esp_err_t e=usb_host_device_free_all();
    if (e!=ESP_OK && e!=ESP_ERR_NOT_FINISHED) return e;
    if (e==ESP_OK) atomic_store(&all_free,true);
    for (unsigned i=0;i<100 && !atomic_load(&all_free);i++) vTaskDelay(pdMS_TO_TICKS(10));
    if (!atomic_load(&all_free)) return ESP_ERR_TIMEOUT;
    atomic_store(&stop_lib,true);
    if (!await_suspend(lib_task)) return ESP_ERR_TIMEOUT;
    e=usb_host_uninstall();
    if (e!=ESP_OK) { atomic_store(&stop_lib,false); vTaskResume(lib_task); return e; }
    vTaskDelete(lib_task); lib_task=NULL; host_ready=false; root_on=false; return ESP_OK;
}
esp_err_t demo_usb_start(char *result, size_t capacity)
{
    esp_err_t e=usb_service_stop();
    if (e!=ESP_OK) return e;
    if (!host_ready) {
        atomic_store(&stop_enum,false); atomic_store(&stop_lib,false); atomic_store(&all_free,false);
        usb_host_config_t cfg={.intr_flags=ESP_INTR_FLAG_LEVEL1};
        e=usb_host_install(&cfg); if(e!=ESP_OK)return e;
        if(xTaskCreate(library,"usb_library",4096,NULL,10,&lib_task)!=pdPASS) {usb_host_uninstall();return ESP_ERR_NO_MEM;}
        host_ready=true; root_on=true;
    }
    if(!root_on) {
        e=usb_host_lib_set_root_port_power(true);
        if(e!=ESP_OK)return e;
        root_on=true;
    }
    if (!client) {
        usb_host_client_config_t cfg={.is_synchronous=false,.max_num_event_msg=8,.async={.client_event_callback=usb_event}};
        e=usb_host_client_register(&cfg,&client); if(e!=ESP_OK)return e;
        if(xTaskCreate(enumerate,"usb_enumerate",4096,NULL,5,&enum_task)!=pdPASS) {
            usb_host_client_deregister(client);client=NULL;return ESP_ERR_NO_MEM;
        }
    }
    snprintf(result,capacity,"USB Host ready. Device HID is detached. Attach a device to the high-speed OTG port.");
    return ESP_OK;
}




