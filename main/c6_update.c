#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include "c6_update.h"
#include "platform.h"
#include "esp_hosted.h"
#include "esp_hosted_ota.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "psa/crypto.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static uint8_t *image;
static size_t expected,used;
static uint8_t expected_hash[32];
static atomic_bool receiving,ready;
bool c6_stage_receiving(void){return atomic_load(&receiving);}
static int nibble(char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;}
esp_err_t c6_stage_command(const char *line)
{
 unsigned n;char hash[65];
 if(platform_busy()||sscanf(line,"c6 stage %u %64s",&n,hash)!=2||n<256||n>1800000||strlen(hash)!=64)return ESP_ERR_INVALID_ARG;
 for(unsigned i=0;i<32;i++){int a=nibble(hash[2*i]),b=nibble(hash[2*i+1]);if(a<0||b<0)return ESP_ERR_INVALID_ARG;expected_hash[i]=(a<<4)|b;}
 atomic_store(&ready,false);free(image);image=heap_caps_malloc(n,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 if(!image)return ESP_ERR_NO_MEM;
 used=0;expected=n;atomic_store(&receiving,true);ESP_LOGI("c6_update","STAGE READY %u",n);return ESP_OK;
}
void c6_stage_feed(const void *bytes,size_t size)
{
 if(!atomic_load(&receiving))return;
 if(size>expected-used)size=expected-used;
 memcpy(image+used,bytes,size);used+=size;
 ESP_LOGI("c6_update","STAGE ACK %u",(unsigned)used);
 if(used!=expected)return;
 uint8_t hash[32];size_t length;
 psa_crypto_init();
 bool valid=psa_hash_compute(PSA_ALG_SHA_256,image,used,hash,sizeof(hash),&length)==PSA_SUCCESS&&length==32&&!memcmp(hash,expected_hash,32);
 valid=valid&&image[0]==0xe9&&image[12]==13&&image[13]==0;
 atomic_store(&ready,valid);atomic_store(&receiving,false);ESP_LOGI("c6_update","STAGE %s %u",valid?"VERIFIED":"INVALID",(unsigned)used);
}
esp_err_t c6_stage_data(const char *line)
{
 unsigned offset;char hex[129];uint8_t bytes[64];
 if(sscanf(line,"c6 data %u %128s",&offset,hex)!=2)return ESP_ERR_INVALID_ARG;
 size_t n=strlen(hex)/2;
 if(!n||strlen(hex)!=n*2||offset>used||offset+n>expected)return ESP_ERR_INVALID_ARG;
 for(size_t i=0;i<n;i++){int a=nibble(hex[2*i]),b=nibble(hex[2*i+1]);if(a<0||b<0)return ESP_ERR_INVALID_ARG;bytes[i]=(a<<4)|b;}
 if(offset<used){
  if(offset+n>used||memcmp(image+offset,bytes,n))return ESP_ERR_INVALID_ARG;
  ESP_LOGI("c6_update","STAGE ACK %u",(unsigned)used);
  if(atomic_load(&ready))ESP_LOGI("c6_update","STAGE VERIFIED %u",(unsigned)used);
  return ESP_OK;
 }
 if(!atomic_load(&receiving))return ESP_ERR_INVALID_STATE;
 c6_stage_feed(bytes,n);return ESP_OK;
}
esp_err_t c6_update(char *out,size_t capacity)
{
 if(!atomic_load(&ready))return ESP_ERR_INVALID_STATE;
 esp_hosted_coprocessor_fwver_t fw;
 esp_err_t e=esp_hosted_get_coprocessor_fwversion(&fw);if(e!=ESP_OK)return e;
 e=esp_hosted_slave_ota_begin();
 for(size_t offset=0;e==ESP_OK&&offset<used;offset+=1024){size_t n=used-offset;if(n>1024)n=1024;e=esp_hosted_slave_ota_write(image+offset,n);}
 if(e==ESP_OK)e=esp_hosted_slave_ota_end();
 if(e==ESP_OK&&(fw.major1>2||(fw.major1==2&&fw.minor1>=6)))e=esp_hosted_slave_ota_activate();
 if(e==ESP_OK){ESP_LOGI("c6_update","OTA VERIFIED; restarting host in 2 seconds");vTaskDelay(pdMS_TO_TICKS(2000));esp_restart();}
 snprintf(out,capacity,"C6 OTA: %s",esp_err_to_name(e));return e;
}



