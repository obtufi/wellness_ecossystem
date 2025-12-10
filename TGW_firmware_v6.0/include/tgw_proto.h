#pragma once

#include "tgw_constants.h"

void tgw_proto_init();

bool tgw_proto_send_to_node(uint8_t node_id, const void* data, size_t len);

bool tgw_proto_poll_rsn_packet(tgw_rx_type_t* type_out,
                               uint8_t* node_id_out,
                               int8_t* rssi_out,
                               uint8_t* buf,
                               size_t* len_in_out);

bool tgw_proto_get_node_mac(uint8_t node_id, uint8_t* mac_out);

void tgw_proto_set_node_mac(uint8_t node_id, const uint8_t* mac);
