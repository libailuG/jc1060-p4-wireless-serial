#include <stdatomic.h>
#include <string.h>
#include "radio.h"
#include "esp_hosted.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/task.h"
#define TD_CFG 0x54440001u
#define TD_TX 0x54440002u
#define TD_RX 0x54440003u
#define TD_STATE 0x54440004u
QueueHandle_t td_radio_rx;
static QueueHandle_t outgoing;
static atomic_bool enabled,reply;
static atomic_int state_error,rssi;
static atomic_uint sent,failed,received,dropped;
static bool initialized;
static StaticQueue_t rx_control,tx_control;
static void input(uint32_t id,const uint8_t *data,size_t size)
{
 if(!atomic_load(&enabled)||size<4)return;
 unsigned n=td_u16(data);
 if(n<TD_HEADER||n>TD_MAX||size!=n+4)return;
 td_packet_t p={.len=n};memcpy(p.bytes,data+4,n);atomic_store(&rssi,(int16_t)td_u16(data+2));
 atomic_fetch_add(&received,1);if(xQueueSend(td_radio_rx,&p,0)!=pdTRUE)atomic_fetch_add(&dropped,1);
}
static void state(uint32_t id,const uint8_t *data,size_t size)
{
 if(size!=8)return;
 atomic_store(&state_error,(int32_t)td_u32(data+4));atomic_store(&enabled,td_u32(data)!=0);atomic_store(&reply,true);
}
static void sender(void *unused)
{
 td_packet_t p;
 while(xQueueReceive(outgoing,&p,portMAX_DELAY)) {
  if(!atomic_load(&enabled))continue;
  esp_err_t e=esp_hosted_send_custom_data(TD_TX,p.bytes,p.len);
  if(e==ESP_OK)atomic_fetch_add(&sent,1);else atomic_fetch_add(&failed,1);
 }
}
void td_radio_init(void)
{
 if(initialized)return;
 uint8_t *r=heap_caps_malloc(32*sizeof(td_packet_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 uint8_t *t=heap_caps_malloc(32*sizeof(td_packet_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 assert(r&&t);td_radio_rx=xQueueCreateStatic(32,sizeof(td_packet_t),r,&rx_control);outgoing=xQueueCreateStatic(32,sizeof(td_packet_t),t,&tx_control);
 assert(td_radio_rx&&outgoing);
 ESP_ERROR_CHECK(esp_hosted_register_custom_callback(TD_RX,input));
 ESP_ERROR_CHECK(esp_hosted_register_custom_callback(TD_STATE,state));
 assert(xTaskCreate(sender,"td_sdio_tx",4096,NULL,5,NULL)==pdPASS);initialized=true;
}
esp_err_t td_radio_enable(bool enable)
{
 td_radio_init();atomic_store(&reply,false);
 uint8_t mode=enable?1:0;
 esp_err_t e=esp_hosted_send_custom_data(TD_CFG,&mode,1);if(e!=ESP_OK)return e;
 int64_t until=esp_timer_get_time()+5000000;
 while(!atomic_load(&reply)&&esp_timer_get_time()<until)vTaskDelay(pdMS_TO_TICKS(10));
 if(!atomic_load(&reply))return ESP_ERR_TIMEOUT;
 if(!enable){xQueueReset(outgoing);xQueueReset(td_radio_rx);}
 return atomic_load(&state_error);
}
bool td_radio_enabled(void){return atomic_load(&enabled);}
bool td_radio_send(const td_packet_t *p){return atomic_load(&enabled)&&xQueueSend(outgoing,p,0)==pdTRUE;}
void td_radio_stats(td_radio_stats_t *o)
{
 *o=(td_radio_stats_t){.sent=sent,.mac_fail=failed,.rx=received,.rx_drop=dropped,.rssi=rssi,.free_heap=heap_caps_get_free_size(MALLOC_CAP_INTERNAL)};
}
esp_err_t td_radio_rate(unsigned code){return ESP_ERR_NOT_SUPPORTED;}
esp_err_t td_radio_test_channel(bool outage){return ESP_ERR_NOT_SUPPORTED;}

