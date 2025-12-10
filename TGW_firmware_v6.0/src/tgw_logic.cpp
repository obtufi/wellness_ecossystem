#include <Arduino.h>
#include <string.h>

#include "tgw_logic.h"
#include "tgw_proto.h"
#include "tgw_store.h"
#include "tgw_uplink.h"
#include "tgw_display.h"

// Framing Serial: [len LSB][len MSB][type][payload...] (payload little-endian)
static const uint8_t UP_RSN_HELLO     = 0xA1;
static const uint8_t UP_RSN_TELEMETRY = 0xA2;
static const uint8_t UP_RSN_CONFIG_ACK= 0xA3;

static const uint8_t DOWN_RSN_CONFIG  = 0xB1;
static const uint8_t DOWN_RSN_HANDSHAKE = 0xB2;

static uint32_t s_last_disp_ms = 0;
static uint32_t s_hello_count = 0;
static uint32_t s_telem_count = 0;

static uint8_t pack_rssi(int8_t rssi) {
    return static_cast<uint8_t>(rssi); // preserva complemento de dois; lado PC deve interpretar como int8
}

static bool send_handshake_to_node(uint8_t node_id) {
    rsn_handshake_packet_t hs = {};
    hs.hdr.type = RSN_PKT_HANDSHAKE;
    hs.hdr.node_id = node_id;
    hs.hdr.mode = RSN_MODE_RUNNING;
    hs.hdr.hw_version = RSN_HW_VERSION;
    hs.hdr.fw_version = RSN_FW_VERSION;
    return tgw_proto_send_to_node(node_id, &hs, sizeof(hs));
}

static void push_hello_to_uplink(const uint8_t* data, size_t len, uint8_t node_id, int8_t rssi) {
    uint8_t frame[RSN_MAX_PACKET_SIZE + 8];
    size_t idx = 0;
    frame[idx++] = UP_RSN_HELLO;
    frame[idx++] = node_id;
    frame[idx++] = pack_rssi(rssi);
    if (idx + len > sizeof(frame)) return;
    memcpy(&frame[idx], data, len);
    idx += len;
    tgw_uplink_send_frame(frame, idx);
}

static void push_config_ack_to_uplink(const uint8_t* data, size_t len, uint8_t node_id, int8_t rssi) {
    uint8_t frame[RSN_MAX_PACKET_SIZE + 8];
    size_t idx = 0;
    frame[idx++] = UP_RSN_CONFIG_ACK;
    frame[idx++] = node_id;
    frame[idx++] = pack_rssi(rssi);
    if (idx + len > sizeof(frame)) return;
    memcpy(&frame[idx], data, len);
    idx += len;
    tgw_uplink_send_frame(frame, idx);
}

static void push_telem_to_uplink(const tgw_telem_item_t& item) {
    uint8_t frame[RSN_MAX_PACKET_SIZE + 16];
    size_t idx = 0;
    frame[idx++] = UP_RSN_TELEMETRY;
    frame[idx++] = item.node_id;
    frame[idx++] = pack_rssi(item.rssi);
    // local_ts_ms
    frame[idx++] = (uint8_t)(item.local_ts_ms & 0xFF);
    frame[idx++] = (uint8_t)((item.local_ts_ms >> 8) & 0xFF);
    frame[idx++] = (uint8_t)((item.local_ts_ms >> 16) & 0xFF);
    frame[idx++] = (uint8_t)((item.local_ts_ms >> 24) & 0xFF);
    size_t payload_len = sizeof(rsn_telemetry_packet_t);
    if (idx + payload_len > sizeof(frame)) return;
    memcpy(&frame[idx], &item.pkt, payload_len);
    idx += payload_len;
    if (!tgw_uplink_send_frame(frame, idx)) {
        if (!tgw_store_push_telem(&item)) {
            Serial.println("[LOGIC] Failed to requeue telemetry after uplink error");
        }
    }
}

void tgw_logic_init() {
    tgw_proto_init();
    tgw_store_init();
    tgw_uplink_init();
    tgw_display_init();
    tgw_display_status("TGW ready", "Listening HELLO");
}

static void handle_rsn_packets() {
    uint8_t buf[RSN_MAX_PACKET_SIZE];
    size_t len = sizeof(buf);
    tgw_rx_type_t type;
    uint8_t node_id;
    int8_t rssi;

    while (tgw_proto_poll_rsn_packet(&type, &node_id, &rssi, buf, &len)) {
        switch (type) {
            case TGW_RX_FROM_RSN_HELLO: {
                s_hello_count++;
                push_hello_to_uplink(buf, len, node_id, rssi);
                char l2[24];
                snprintf(l2, sizeof(l2), "HELLO n:%u rssi:%d", node_id, rssi);
                tgw_display_status("HELLO rx", l2);
                break;
            }
            case TGW_RX_FROM_RSN_TELEMETRY: {
                if (len != sizeof(rsn_telemetry_packet_t)) {
                    Serial.println("[LOGIC] Dropping telemetry with size mismatch");
                    break;
                }
                s_telem_count++;
                tgw_telem_item_t item = {};
                item.node_id = node_id;
                item.rssi = rssi;
                item.local_ts_ms = millis();
                memcpy(&item.pkt, buf, sizeof(rsn_telemetry_packet_t));
                if (tgw_uplink_is_connected()) {
                    push_telem_to_uplink(item);
                } else {
                    if (!tgw_store_push_telem(&item)) {
                        Serial.println("[LOGIC] Telemetry queue full; dropping newest item");
                    }
                }
                if (millis() - s_last_disp_ms > 1000) {
                    char l2[26];
                    snprintf(l2, sizeof(l2), "TELEM n:%u rssi:%d", node_id, rssi);
                    tgw_display_status("Telem rx", l2);
                    s_last_disp_ms = millis();
                }
                // Atualiza OLED com resumo do último pacote
                tgw_display_summary(s_hello_count,
                                    s_telem_count,
                                    item.node_id,
                                    item.pkt.cycle,
                                    item.pkt.soil_mean_raw,
                                    item.pkt.vbat_mean_raw,
                                    item.rssi);
                break;
            }
            case TGW_RX_FROM_RSN_CONFIG_ACK: {
                push_config_ack_to_uplink(buf, len, node_id, rssi);
                tgw_display_status("Config ACK", "forwarded");
                break;
            }
            case TGW_RX_FROM_RSN_DEBUG:
            default:
                break;
        }
        len = sizeof(buf);
    }
}

static void handle_uplink_frames() {
    uint8_t buf[RSN_MAX_PACKET_SIZE + 8];
    size_t len = sizeof(buf);
    while (tgw_uplink_poll_frame(buf, &len)) {
        if (len < 1) {
            len = sizeof(buf);
            continue;
        }
        uint8_t type = buf[0];
        if (type == DOWN_RSN_CONFIG) {
            if (len < 2 + sizeof(rsn_config_packet_t)) {
                len = sizeof(buf);
                continue;
            }
            uint8_t node_id = buf[1];
            rsn_config_packet_t cfg = {};
            memcpy(&cfg, &buf[2], sizeof(rsn_config_packet_t));
            cfg.hdr.type = RSN_PKT_CONFIG;
            cfg.hdr.node_id = node_id;
            cfg.hdr.mode = RSN_MODE_RUNNING;
            cfg.hdr.hw_version = RSN_HW_VERSION;
            cfg.hdr.fw_version = RSN_FW_VERSION;
            send_handshake_to_node(node_id); // melhor esforco para sair do pairing
            tgw_store_save_node_config(node_id, &cfg);
            tgw_proto_send_to_node(node_id, &cfg, sizeof(rsn_config_packet_t));
            tgw_display_status("Send CONFIG", "to RSN");
        } else if (type == DOWN_RSN_HANDSHAKE) {
            if (len < 2) {
                len = sizeof(buf);
                continue;
            }
            uint8_t node_id = buf[1];
            rsn_handshake_packet_t hs = {};
            if (len >= 2 + sizeof(rsn_handshake_packet_t)) {
                memcpy(&hs, &buf[2], sizeof(rsn_handshake_packet_t));
            }
            hs.hdr.type = RSN_PKT_HANDSHAKE;
            hs.hdr.node_id = node_id;
            hs.hdr.mode = RSN_MODE_RUNNING;
            hs.hdr.hw_version = RSN_HW_VERSION;
            hs.hdr.fw_version = RSN_FW_VERSION;
            bool ok = tgw_proto_send_to_node(node_id, &hs, sizeof(rsn_handshake_packet_t));
            tgw_display_status(ok ? "Send HANDSHAKE" : "Handshake fail", ok ? "to RSN" : "no MAC?");
        }
        len = sizeof(buf);
    }
}

static void flush_pending_telem() {
    if (!tgw_uplink_is_connected()) return;
    tgw_telem_item_t item;
    while (tgw_store_has_pending_telem()) {
        if (!tgw_store_pop_telem(&item)) break;
        push_telem_to_uplink(item);
    }
}

void tgw_logic_step() {
    handle_rsn_packets();
    handle_uplink_frames();
    flush_pending_telem();
}
