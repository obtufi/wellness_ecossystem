// ESP-NOW and packet helpers (assembly/parsing).
#pragma once

#include "constants.h"
#include "globals.h"

void proto_init();

void proto_build_hello_packet(rsn_hello_packet_t* pkt);
void proto_build_telemetry_packet(rsn_telemetry_packet_t* pkt, const rsn_adc_stats_t* soil, const rsn_adc_stats_t* vbat, const rsn_adc_stats_t* ntc);
bool proto_apply_config_from_packet(const rsn_config_packet_t* pkt);
bool proto_handle_handshake_packet(const rsn_handshake_packet_t* pkt);

bool proto_send_hello();
bool proto_send_telemetry();
bool proto_send_config_ack(uint8_t status);
bool proto_last_send_ok();

bool proto_try_receive_handshake(rsn_handshake_packet_t* hello_in);
bool proto_try_receive_config(rsn_config_packet_t* cfg_out);

void proto_fill_header(rsn_header_t* hdr, rsn_packet_type_t type);
