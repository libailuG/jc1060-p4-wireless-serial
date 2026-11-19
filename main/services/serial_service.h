#pragma once
#include "serial_frame.h"
#include "esp_err.h"
#include "serial_history.h"
typedef struct {
    serial_frame_t received;
    uint32_t rx, tx, discarded, bad_tail;
    bool active, continuous;
    esp_err_t error;
} serial_status_t;
esp_err_t serial_service_init(void);
void serial_service_enable(bool enabled);
void serial_service_repeat(bool enabled);
bool serial_service_send(const serial_frame_t *frame, bool continuous);
void serial_service_snapshot(serial_status_t *out);
size_t serial_service_read(uint32_t *cursor, serial_sample_t *out, size_t capacity, uint32_t *lost);
