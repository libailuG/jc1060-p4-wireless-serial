#pragma once
#include "wire.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
typedef struct { uint32_t sent,mac_fail,rx,rx_drop,tx_drop,bad,free_heap,min_heap,version; int rssi; } td_radio_stats_t;
extern QueueHandle_t td_radio_rx;
void td_radio_init(void);
bool td_radio_send(const td_packet_t *p);
void td_radio_stats(td_radio_stats_t *out);
esp_err_t td_radio_rate(unsigned code);
esp_err_t td_radio_test_channel(bool outage);

esp_err_t td_radio_enable(bool enable);
bool td_radio_enabled(void);
