#include "wire.h"
#include <string.h>
uint16_t td_u16(const uint8_t*p){return p[0]|((uint16_t)p[1]<<8);}
uint32_t td_u32(const uint8_t*p){return td_u16(p)|((uint32_t)td_u16(p+2)<<16);}
uint64_t td_u64(const uint8_t*p){return td_u32(p)|((uint64_t)td_u32(p+4)<<32);}
void td_w16(uint8_t*p,uint16_t v){p[0]=v;p[1]=v>>8;}
void td_w32(uint8_t*p,uint32_t v){td_w16(p,v);td_w16(p+2,v>>16);}
void td_w64(uint8_t*p,uint64_t v){td_w32(p,v);td_w32(p+4,v>>32);}
uint32_t td_crc(uint32_t prior,const void *data,size_t n){
 const uint8_t *p=data;uint32_t c=~prior;
 while(n--){c^=*p++;for(int i=0;i<8;i++)c=(c>>1)^(0x82f63b78u&-(c&1u));}
 return ~c;
}
void td_make(td_packet_t*p,uint8_t type,uint16_t flags,uint64_t session,uint64_t seq,uint16_t count,uint32_t epoch,const void*data,uint16_t size){
 memset(p->bytes,0,TD_HEADER);p->len=TD_HEADER+size;
 memcpy(p->bytes,"TDR1",4);p->bytes[4]=1;p->bytes[5]=type;
 td_w16(p->bytes+6,flags);td_w64(p->bytes+8,session);td_w64(p->bytes+16,seq);
 td_w16(p->bytes+24,count);td_w16(p->bytes+26,size);td_w32(p->bytes+32,epoch);
 if(size)memcpy(p->bytes+40,data,size);
 td_w32(p->bytes+28,td_crc(0,p->bytes,p->len));
}
bool td_parse(const td_packet_t*p,td_header_t*h){
 if(p->len<TD_HEADER||p->len>TD_MAX||memcmp(p->bytes,"TDR1",4)||p->bytes[4]!=1)return false;
 uint16_t size=td_u16(p->bytes+26);if(size+TD_HEADER!=p->len)return false;
 uint32_t crc=td_crc(0,p->bytes,28);uint32_t zero=0;crc=td_crc(crc,&zero,4);crc=td_crc(crc,p->bytes+32,p->len-32);
 if(crc!=td_u32(p->bytes+28)||td_u32(p->bytes+36))return false;
 h->type=p->bytes[5];h->flags=td_u16(p->bytes+6);h->session=td_u64(p->bytes+8);
 h->seq=td_u64(p->bytes+16);h->count=td_u16(p->bytes+24);h->size=size;h->epoch=td_u32(p->bytes+32);h->payload=p->bytes+40;
 if(h->type==TD_DATA&&(h->count!=1||!h->size||h->size>960||h->seq==UINT64_MAX))return false;
 return h->type>=TD_HELLO&&h->type<=TD_PROBE;
}
size_t td_cobs_encode(const uint8_t*src,size_t n,uint8_t*out){size_t pos=1,codepos=0;uint8_t code=1;
 for(size_t i=0;i<n;i++){if(!src[i]){out[codepos]=code;codepos=pos++;code=1;}else{out[pos++]=src[i];if(++code==255){out[codepos]=code;codepos=pos++;code=1;}}}
 out[codepos]=code;out[pos++]=0;return pos;
}
size_t td_cobs_decode(const uint8_t*src,size_t n,uint8_t*out,size_t cap){size_t i=0,k=0;
 while(i<n){uint8_t code=src[i++];if(!code||i+code-1>n||k+code-1>cap)return 0;
  for(int j=1;j<code;j++)out[k++]=src[i++];
  if(code!=255&&i<n){if(k==cap)return 0;out[k++]=0;}}
 return k;
}
void td_demo(uint64_t seq,uint8_t out[TD_FRAME]){
 td_w32(out,0xaaaaaaaau);td_w32(out+4,(uint32_t)seq);td_w32(out+8,(uint32_t)seq^0x5a5a5a5au);
 for(unsigned i=0;i<8;i++){float f=(float)((int32_t)(seq%10000)-5000)*(float)(i+1)/128.0f;uint32_t bits;memcpy(&bits,&f,4);td_w32(out+12+i*4,bits);}
 td_w32(out+44,0xeeeeeeeeu);
}
