#pragma once
#include "serial_uart.h"
bool serial_transport_active(void);
bool serial_transport_send(const serial_frame_t *f);
void serial_transport_snapshot(serial_uart_snapshot_t *o);
size_t serial_transport_history(uint32_t *c,serial_sample_t *o,size_t n,uint32_t *l);
const char *serial_transport_status(void);
void serial_transport_start(void);
void serial_transport_report(void);
