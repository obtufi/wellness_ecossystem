#pragma once

#include <stddef.h>
#include <stdint.h>

void tgw_uplink_init();
bool tgw_uplink_is_connected();
bool tgw_uplink_send_frame(const uint8_t* data, size_t len);
bool tgw_uplink_poll_frame(uint8_t* buf, size_t* len_in_out);
