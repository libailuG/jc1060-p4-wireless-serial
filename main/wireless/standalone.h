#pragma once
#include "serial_frame.h"
void serial_ui_send_frame(const serial_frame_t *f);
void serial_ui_view(unsigned tab);
void serial_ui_report(void);
