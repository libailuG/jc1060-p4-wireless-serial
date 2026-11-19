#include "serial_transport.h"
#include "radio.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "driver/uart.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#define WINDOW 16
typedef struct {uint64_t seq;uint16_t len;uint8_t data[960];bool valid;} slot_t;
static slot_t *slots;
static uint64_t session,next,received_bytes,duplicates,tx_next,tx_limit;
static uint32_t epoch,crc,bad,tx_crc,tx_pending_crc,tx_retries;
static int64_t last_peer,last_ack,tx_sent;
static bool tx_pending;
static uint8_t tx_data[48];
static QueueHandle_t tx_queue;
static serial_parser_t parser;
static serial_uart_snapshot_t snapshot;

static atomic_bool online;
static portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
static td_packet_t out;
static struct {uint64_t bytes,packets,duplicates;uint32_t crc,bad,retries;int rssi;} stats;

bool serial_transport_active(void){return true;}
const char *serial_transport_status(void){return td_radio_enabled()&&atomic_load(&online)?"ESP-NOW ready":"ESP-NOW waiting";}
bool serial_transport_send(const serial_frame_t *f){
 if(!td_radio_enabled()||!atomic_load(&online))return false;
 uint8_t b[48];serial_frame_encode(f,b);return xQueueSend(tx_queue,b,0)==pdTRUE;
}
void serial_transport_snapshot(serial_uart_snapshot_t *o){portENTER_CRITICAL(&mux);*o=snapshot;portEXIT_CRITICAL(&mux);}
size_t serial_transport_history(uint32_t *c,serial_sample_t *o,size_t n,uint32_t *l){
 (void)c;(void)o;(void)n;(void)l;return 0; /* Wireless curve sampling disabled. */
}
static void frame_received(const serial_frame_t *f,void *ctx){
 (void)ctx;
 portENTER_CRITICAL(&mux);snapshot.last=*f;snapshot.has_frame=true;snapshot.rx++;
 portEXIT_CRITICAL(&mux);
}
static void acknowledge(void){
 uint8_t b[184]={0};td_w64(b,next);td_w64(b+8,next+WINDOW);td_w32(b+16,crc);
 unsigned nr=0;uint64_t q=next;
 while(q<next+WINDOW){
  if(!slots[q%WINDOW].valid||slots[q%WINDOW].seq!=q){q++;continue;}
  uint64_t start=q;while(q<next+WINDOW&&slots[q%WINDOW].valid&&slots[q%WINDOW].seq==q)q++;
  td_w64(b+24+nr*10,start);td_w16(b+32+nr*10,q-start);nr++;
 }
 td_w16(b+20,nr);td_make(&out,TD_ACK,0,session,next,0,epoch,b,24+nr*10);
 td_radio_send(&out);last_ack=esp_timer_get_time();
}
static void input(const td_packet_t *p){
 td_header_t h;if(!td_parse(p,&h)){bad++;return;}
 if(h.type==TD_HELLO){
  if(h.flags!=0x100||h.count||h.size||h.epoch||!h.session)return;
  if(h.session!=session){
   atomic_store(&online,false);xQueueReset(tx_queue);
   session=h.session;next=received_bytes=duplicates=tx_next=tx_limit=0;crc=bad=tx_crc=tx_retries=0;tx_pending=false;tx_sent=0;
   memset(slots,0,WINDOW*sizeof(*slots));memset(&parser,0,sizeof(parser));
   /* Keep the UI history sequence monotonic across source reconnects. */
  }
  last_peer=esp_timer_get_time();atomic_store(&online,true);acknowledge();return;
 }
 if(!session||h.session!=session||h.epoch!=epoch)return;
 if(h.type==TD_ACK&&h.flags==1&&h.size>=24){
  unsigned nr=td_u16(h.payload+20);uint64_t n=td_u64(h.payload),credit=td_u64(h.payload+8);
  if(nr>16||h.size!=24+10*nr||credit<n||credit-n>WINDOW||n<tx_next||n>tx_next+(tx_pending?1:0))return;
  uint32_t expected=n==tx_next?tx_crc:tx_pending_crc;
  if(td_u32(h.payload+16)!=expected){bad++;return;}
  if(n>tx_next){
   tx_next=n;tx_crc=expected;tx_pending=false;tx_sent=0;
   portENTER_CRITICAL(&mux);snapshot.tx++;portEXIT_CRITICAL(&mux);
  }
  tx_limit=credit;last_peer=esp_timer_get_time();return;
 }
 if(h.type!=TD_DATA||h.flags)return;
 last_peer=esp_timer_get_time();
 if(h.seq<next){duplicates++;acknowledge();return;}
 if(h.seq-next>=WINDOW){acknowledge();return;}
 slot_t *s=&slots[h.seq%WINDOW];
 if(s->valid){duplicates++;acknowledge();return;}
 s->seq=h.seq;s->len=h.size;memcpy(s->data,h.payload,h.size);s->valid=true;
 while(slots[next%WINDOW].valid&&slots[next%WINDOW].seq==next){
  s=&slots[next%WINDOW];serial_parser_feed(&parser,s->data,s->len,frame_received,NULL);
  crc=td_crc(crc,s->data,s->len);received_bytes+=s->len;s->valid=false;next++;
 }
 portENTER_CRITICAL(&mux);snapshot.discarded=parser.discarded;snapshot.bad_tail=parser.bad_tail;portEXIT_CRITICAL(&mux);
 acknowledge();
}
static void worker(void *arg){
 (void)arg;td_packet_t p;int64_t last_publish=0;
 for(;;){
  if(xQueueReceive(td_radio_rx,&p,pdMS_TO_TICKS(1))==pdTRUE)input(&p);
  int64_t now=esp_timer_get_time();bool linked=session&&now-last_peer<2000000;atomic_store(&online,linked);
  if(session&&now-last_ack>=100000)acknowledge();
  if(linked&&tx_next<tx_limit){
   if(!tx_pending&&xQueueReceive(tx_queue,tx_data,0)==pdTRUE){tx_pending=true;tx_sent=0;tx_pending_crc=td_crc(tx_crc,tx_data,48);}
   if(tx_pending&&(!tx_sent||now-tx_sent>=120000)){
    td_make(&out,TD_DATA,1,session,tx_next,1,epoch,tx_data,48);
    if(td_radio_send(&out)){if(tx_sent)tx_retries++;tx_sent=now;}
   }
  }
  if(now-last_publish>=100000){
   td_radio_stats_t r;td_radio_stats(&r);portENTER_CRITICAL(&mux);
   stats.bytes=received_bytes;stats.packets=next;stats.duplicates=duplicates;stats.crc=crc;stats.bad=bad;stats.retries=tx_retries;stats.rssi=r.rssi;
   portEXIT_CRITICAL(&mux);last_publish=now;
  }
 }
}
void serial_transport_report(void){
 serial_uart_snapshot_t s;char hex[97],b[600];uint8_t raw[48];uint64_t bytes,packets;uint32_t checksum,errors,retries;int rssi;
 portENTER_CRITICAL(&mux);s=snapshot;bytes=stats.bytes;packets=stats.packets;checksum=stats.crc;errors=stats.bad;retries=stats.retries;rssi=stats.rssi;portEXIT_CRITICAL(&mux);
 serial_frame_encode(&s.last,raw);for(unsigned i=0;i<48;i++)snprintf(hex+2*i,3,"%02x",raw[i]);
 int n=snprintf(b,sizeof(b),"{\"fw\":\"1.3.0-wireless\",\"rx_frames\":%lu,\"tx_acked_frames\":%lu,\"rx_bytes\":%"PRIu64",\"packets\":%"PRIu64",\"crc\":%lu,\"bad_packets\":%lu,\"bad_tail\":%lu,\"discarded\":%lu,\"tx_retries\":%lu,\"online\":%d,\"rssi\":%d,\"last_hex\":\"%s\"}\r\n",(unsigned long)s.rx,(unsigned long)s.tx,bytes,packets,(unsigned long)checksum,(unsigned long)errors,(unsigned long)s.bad_tail,(unsigned long)s.discarded,(unsigned long)retries,atomic_load(&online)?1:0,rssi,hex);
 if(n>0&&n<(int)sizeof(b))printf("%.*s",n,b);
}
void serial_transport_start(void){
 static bool started;if(started)return;started=true;
 slots=heap_caps_calloc(WINDOW,sizeof(*slots),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
 assert(slots);
 tx_queue=xQueueCreate(64,48);assert(tx_queue);serial_frame_init(&snapshot.last);
 epoch=esp_random();if(!epoch)epoch=1;td_radio_init();
 assert(xTaskCreatePinnedToCore(worker,"wireless",8192,NULL,5,NULL,0)==pdPASS);
}


