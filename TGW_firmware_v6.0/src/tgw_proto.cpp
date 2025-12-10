#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <string.h>

#include "tgw_proto.h"

typedef struct {
    tgw_rx_type_t type;
    uint8_t       node_id;
    int8_t        rssi;
    uint16_t      len;
    uint8_t       data[RSN_MAX_PACKET_SIZE];
} tgw_rx_entry_t;

static const uint8_t RX_FIFO_LEN = 16;
static tgw_rx_entry_t s_rx_fifo[RX_FIFO_LEN];
static volatile uint8_t s_rx_head = 0;
static volatile uint8_t s_rx_tail = 0;
static uint32_t s_last_overflow_ms = 0;

static tgw_node_status_t s_nodes[TGW_MAX_NODES];
static uint8_t s_last_unset_mac[6] = {};
static uint32_t s_last_unset_ms = 0;
static bool s_has_unset_mac = false;
static const uint32_t UNPAIRED_MAC_TTL_MS = 8000;

static const char* pkt_type_str(uint8_t t) {
    switch (t) {
        case RSN_PKT_HELLO: return "HELLO";
        case RSN_PKT_TELEMETRY: return "TELEM";
        case RSN_PKT_CONFIG_ACK: return "CFG_ACK";
        case RSN_PKT_CONFIG: return "CFG";
        case RSN_PKT_HANDSHAKE: return "HS";
        default: return "UNK";
    }
}

static void log_mac(const char* prefix, const uint8_t* mac) {
    if (!mac) return;
    Serial.printf("%s %02X:%02X:%02X:%02X:%02X:%02X\n", prefix,
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static bool mac_is_unknown(const uint8_t* mac) {
    if (!mac) return true;
    bool all_ff = true;
    bool all_00 = true;
    for (uint8_t i = 0; i < 6; ++i) {
        if (mac[i] != 0xFF) all_ff = false;
        if (mac[i] != 0x00) all_00 = false;
    }
    return all_ff || all_00;
}

static bool tgw_proto_push(const tgw_rx_entry_t& e) {
    uint8_t next = (s_rx_head + 1u) % RX_FIFO_LEN;
    if (next == s_rx_tail) {
        const uint32_t now = millis();
        if (now - s_last_overflow_ms > 500) {
            Serial.println("[PROTO] RX FIFO overflow; dropping packet");
            s_last_overflow_ms = now;
        }
        return false; // fifo cheia
    }
    s_rx_fifo[s_rx_head] = e;
    s_rx_head = next;
    return true;
}

static bool tgw_proto_pop(tgw_rx_entry_t* e) {
    if (s_rx_tail == s_rx_head) return false;
    *e = s_rx_fifo[s_rx_tail];
    s_rx_tail = (s_rx_tail + 1u) % RX_FIFO_LEN;
    return true;
}

static tgw_node_status_t* find_node(uint8_t node_id) {
    for (uint8_t i = 0; i < TGW_MAX_NODES; ++i) {
        if (s_nodes[i].in_use && s_nodes[i].node_id == node_id) {
            return &s_nodes[i];
        }
    }
    return nullptr;
}

static tgw_node_status_t* allocate_node(uint8_t node_id) {
    tgw_node_status_t* slot = find_node(node_id);
    if (slot) return slot;
    for (uint8_t i = 0; i < TGW_MAX_NODES; ++i) {
        if (!s_nodes[i].in_use) {
            s_nodes[i].in_use = true;
            s_nodes[i].node_id = node_id;
            memset(s_nodes[i].mac, 0xFF, sizeof(s_nodes[i].mac));
            return &s_nodes[i];
        }
    }
    return nullptr;
}

static void on_send(const uint8_t*, esp_now_send_status_t) {
    // Nada a fazer aqui por enquanto; poderia sinalizar status se precisarmos.
}

static void on_recv(const uint8_t* mac, const uint8_t* data, int len) {
    if (!data || len <= 0 || len > RSN_MAX_PACKET_SIZE) {
        Serial.printf("[TGW] RX invalid len=%d\n", len);
        return;
    }
    if (len < static_cast<int>(sizeof(rsn_header_t))) {
        Serial.println("[TGW] RX too short for header");
        return;
    }

    tgw_rx_entry_t entry = {};
    memcpy(entry.data, data, len);
    entry.len = static_cast<uint16_t>(len);
    entry.rssi = WiFi.RSSI(); // melhor estimativa disponivel

    const rsn_header_t* hdr = reinterpret_cast<const rsn_header_t*>(data);
    entry.node_id = hdr ? hdr->node_id : RSN_NODE_ID_UNSET;

    switch (hdr ? hdr->type : 0) {
        case RSN_PKT_HELLO:      entry.type = TGW_RX_FROM_RSN_HELLO; break;
        case RSN_PKT_TELEMETRY:  entry.type = TGW_RX_FROM_RSN_TELEMETRY; break;
        case RSN_PKT_CONFIG_ACK: entry.type = TGW_RX_FROM_RSN_CONFIG_ACK; break;
        case RSN_PKT_DEBUG:      entry.type = TGW_RX_FROM_RSN_DEBUG; break;
        default:                 entry.type = TGW_RX_FROM_RSN_NONE; break;
    }

    // Atualiza MAC do node para futuras transmissoes
    if (mac) {
        if (entry.node_id != RSN_NODE_ID_UNSET) {
            tgw_node_status_t* st = allocate_node(entry.node_id);
            if (st) {
                memcpy(st->mac, mac, 6);
                st->last_rssi = entry.rssi;
                st->last_seen_ms = millis();
            }
        } else {
            memcpy(s_last_unset_mac, mac, 6);
            s_last_unset_ms = millis();
            s_has_unset_mac = true;
            tgw_node_status_t* st = allocate_node(RSN_NODE_ID_UNSET);
            if (st) {
                memcpy(st->mac, mac, 6);
                st->last_rssi = entry.rssi;
                st->last_seen_ms = millis();
            }
        }
    }

    if (mac) {
        Serial.printf("[TGW] RX type=%s(0x%02X) node=%u len=%d rssi=%d mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
                      pkt_type_str(hdr ? hdr->type : 0), hdr ? hdr->type : 0, entry.node_id, len, entry.rssi,
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }

    tgw_proto_push(entry);
}

void tgw_proto_init() {
    memset(s_nodes, 0, sizeof(s_nodes));
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        Serial.printf("[TGW] esp_now_init failed err=%d\n", (int)err);
        return;
    }
    esp_now_register_send_cb(on_send);
    esp_now_register_recv_cb(on_recv);
}

bool tgw_proto_send_to_node(uint8_t node_id, const void* data, size_t len) {
    if (!data || len == 0 || len > RSN_MAX_PACKET_SIZE) return false;

    const uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    const uint8_t* dest_mac = broadcast;
    uint8_t dest_mac_buf[6] = {};
    bool used_unpaired_mac = false;

    tgw_node_status_t* st = find_node(node_id);
    if (st && !mac_is_unknown(st->mac)) {
        dest_mac = st->mac;
    } else if (node_id != RSN_NODE_ID_UNSET && s_has_unset_mac) {
        const uint32_t now = millis();
        if (now - s_last_unset_ms <= UNPAIRED_MAC_TTL_MS) {
            memcpy(dest_mac_buf, s_last_unset_mac, sizeof(dest_mac_buf));
            dest_mac = dest_mac_buf;
            used_unpaired_mac = true;
        }
    }

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, dest_mac, 6);
    peer.channel = 0;
    peer.encrypt = false;

    if (!esp_now_is_peer_exist(peer.peer_addr)) {
        esp_err_t perr = esp_now_add_peer(&peer);
        if (perr != ESP_OK) {
            Serial.printf("[TGW] add_peer failed err=%d mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
                          (int)perr, dest_mac[0], dest_mac[1], dest_mac[2],
                          dest_mac[3], dest_mac[4], dest_mac[5]);
        }
    }

    Serial.printf("[TGW] TX node=%u len=%u mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  node_id, (unsigned)len,
                  dest_mac[0], dest_mac[1], dest_mac[2],
                  dest_mac[3], dest_mac[4], dest_mac[5]);
    esp_err_t serr = esp_now_send(dest_mac, reinterpret_cast<const uint8_t*>(data), len);
    const bool sent = (serr == ESP_OK);
    if (!sent) {
        Serial.printf("[TGW] esp_now_send failed err=%d\n", (int)serr);
    }
    if (used_unpaired_mac && sent) {
        tgw_proto_set_node_mac(node_id, dest_mac);
        s_has_unset_mac = false;
    }
    return sent;
}

bool tgw_proto_poll_rsn_packet(tgw_rx_type_t* type_out,
                               uint8_t* node_id_out,
                               int8_t* rssi_out,
                               uint8_t* buf,
                               size_t* len_in_out) {
    if (!type_out || !node_id_out || !rssi_out || !buf || !len_in_out) return false;

    tgw_rx_entry_t e;
    if (!tgw_proto_pop(&e)) return false;

    *type_out    = e.type;
    *node_id_out = e.node_id;
    *rssi_out    = e.rssi;

    size_t copy_len = (*len_in_out < (size_t)e.len) ? *len_in_out : (size_t)e.len;
    memcpy(buf, e.data, copy_len);
    *len_in_out = copy_len;
    return true;
}

bool tgw_proto_get_node_mac(uint8_t node_id, uint8_t* mac_out) {
    if (!mac_out) return false;
    tgw_node_status_t* st = find_node(node_id);
    if (!st) return false;
    memcpy(mac_out, st->mac, 6);
    return true;
}

void tgw_proto_set_node_mac(uint8_t node_id, const uint8_t* mac) {
    if (!mac) return;
    tgw_node_status_t* st = allocate_node(node_id);
    if (st) {
        memcpy(st->mac, mac, 6);
    }
}
