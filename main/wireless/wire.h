#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#define TD_HEADER 40
#define TD_FRAMES 20
#define TD_FRAME 48
#define TD_MAX (TD_HEADER+TD_FRAMES*TD_FRAME)
#define TD_WINDOW 16
#define TD_SLOTS (TD_WINDOW*TD_FRAMES)
enum { TD_HELLO=1, TD_DATA=2, TD_ACK=3, TD_CONFIG=4, TD_STATUS=5, TD_RESET=6, TD_PROBE=7 };
typedef struct { uint16_t len; uint8_t bytes[TD_MAX]; } td_packet_t;
typedef struct { uint8_t type; uint16_t flags,count,size; uint64_t session,seq; uint32_t epoch; const uint8_t *payload; } td_header_t;
uint16_t td_u16(const uint8_t *p);
uint32_t td_u32(const uint8_t *p);
uint64_t td_u64(const uint8_t *p);
void td_w16(uint8_t *p,uint16_t v);
void td_w32(uint8_t *p,uint32_t v);
void td_w64(uint8_t *p,uint64_t v);
uint32_t td_crc(uint32_t prior,const void *p,size_t n);
void td_make(td_packet_t *p,uint8_t type,uint16_t flags,uint64_t session,uint64_t seq,uint16_t count,uint32_t epoch,const void *data,uint16_t size);
bool td_parse(const td_packet_t *p,td_header_t *h);
size_t td_cobs_encode(const uint8_t *src,size_t n,uint8_t *out);
size_t td_cobs_decode(const uint8_t *src,size_t n,uint8_t *out,size_t cap);
void td_demo(uint64_t seq,uint8_t out[TD_FRAME]);
