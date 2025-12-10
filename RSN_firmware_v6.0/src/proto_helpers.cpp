#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_system.h>
#include <string.h>

#include "proto_helpers.h"

static bool     s_proto_ready     = false;
// Use TGW MAC known a priori; fallback to broadcast if you want auto-discovery.
static const uint8_t s_tgw_mac[6] = {0xA8, 0x42, 0xE3, 0x4A, 0xA4, 0x24};
static const uint8_t s_rsn_mac_fixed[6] = {0x24, 0x0A, 0xC4, 0x12, 0x34, 0x57};
static uint8_t  s_peer_mac[6]     = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t  s_espnow_channel  = 1;
static volatile bool s_last_send_ok = false;

static bool ensure_peer_added();

struct LastPacket {
    bool              has_packet;
    rsn_packet_type_t type;
    int               len;
};

static volatile LastPacket s_last_pkt = {false, RSN_PKT_HELLO, 0};

static void on_send(const uint8_t*, esp_now_send_status_t status) {
    s_last_send_ok = (status == ESP_NOW_SEND_SUCCESS);
    Serial.printf("[RSN] on_send status=%d\n", (int)status);
}

static void on_recv(const uint8_t* mac, const uint8_t* data, int len) {
    if (len <= 0 || len > RSN_MAX_PACKET_SIZE) {
        Serial.printf("[RSN] on_recv invalid len=%d\n", len);
        return;
    }

    if (mac != nullptr) {
        memcpy(s_peer_mac, mac, sizeof(s_peer_mac));
        ensure_peer_added();
    }

    memcpy(g_rx_buffer, data, len);
    s_last_pkt.len        = len;
    s_last_pkt.has_packet = true;
    s_last_pkt.type       = static_cast<rsn_packet_type_t>(g_rx_buffer[0]);
    Serial.printf("[RSN] RX type=0x%02X len=%d mac=%02X:%02X:%02X:%02X:%02X:%02X\n",
                  s_last_pkt.type, len,
                  s_peer_mac[0], s_peer_mac[1], s_peer_mac[2],
                  s_peer_mac[3], s_peer_mac[4], s_peer_mac[5]);
}

static bool ensure_peer_added() {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, s_peer_mac, sizeof(s_peer_mac));
    peer.channel = s_espnow_channel;
    peer.encrypt = false;
    peer.ifidx = WIFI_IF_STA;

    if (esp_now_is_peer_exist(peer.peer_addr)) {
        return true;
    }

    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK) {
        Serial.printf("[RSN] add_peer failed err=%d\n", (int)err);
    }
    return err == ESP_OK;
}

void proto_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    esp_wifi_start();
    esp_wifi_set_channel(s_espnow_channel, WIFI_SECOND_CHAN_NONE);
    uint8_t mac[6] = {};
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    // If MAC invalid (all zero), set fixed RSN MAC to ensure ESP-NOW works.
    bool mac_invalid = true;
    for (int i = 0; i < 6; ++i) {
        if (mac[i] != 0x00) { mac_invalid = false; break; }
    }
    if (mac_invalid) {
        esp_wifi_set_mac(WIFI_IF_STA, s_rsn_mac_fixed);
        memcpy(mac, s_rsn_mac_fixed, 6);
    }

    esp_err_t err = esp_now_init();
    s_proto_ready = (err == ESP_OK);
    if (!s_proto_ready) {
        Serial.printf("[RSN] esp_now_init failed err=%d\n", (int)err);
        return;
    }
    Serial.printf("[RSN] WiFi channel=%u MAC=%02X:%02X:%02X:%02X:%02X:%02X (peer bcast)\n",
                  s_espnow_channel,
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    esp_now_register_send_cb(on_send);
    esp_now_register_recv_cb(on_recv);

    ensure_peer_added(); // tenta adicionar broadcast/peer inicial, mas nao bloqueia envios
}

void proto_fill_header(rsn_header_t* hdr, rsn_packet_type_t type) {
    if (!hdr) {
        return;
    }

    hdr->type       = static_cast<uint8_t>(type);
    hdr->node_id    = g_rsn_status.node_id;
    hdr->mode       = static_cast<uint8_t>(g_rsn_mode);
    hdr->hw_version = RSN_HW_VERSION;
    hdr->fw_version = RSN_FW_VERSION;
}

static bool proto_send_packet(const void* payload, size_t len) {
    if (!s_proto_ready || payload == nullptr || len == 0 || len > RSN_MAX_PACKET_SIZE) {
        return false;
    }

    const rsn_header_t* hdr = reinterpret_cast<const rsn_header_t*>(payload);
    memcpy(g_tx_buffer, payload, len);
    s_last_send_ok = false;
    // For debug, force broadcast to avoid peer add issues until link is stable.
    const uint8_t* dest = nullptr;
    Serial.printf("[RSN] TX type=0x%02X len=%u dest=(bcast)\n",
                  hdr ? hdr->type : 0xFF, (unsigned)len);
    esp_err_t err = esp_now_send(dest, g_tx_buffer, len);
    if (err != ESP_OK) {
        Serial.printf("[RSN] esp_now_send failed err=%d\n", (int)err);
    }
    return err == ESP_OK;
}

void proto_build_hello_packet(rsn_hello_packet_t* pkt) {
    if (!pkt) {
        return;
    }
    memset(pkt, 0, sizeof(*pkt));
    proto_fill_header(&pkt->hdr, RSN_PKT_HELLO);
    pkt->capabilities = (RSN_CAP_SOIL | RSN_CAP_VBAT | RSN_CAP_NTC | RSN_CAP_RGB);
}

void proto_build_telemetry_packet(rsn_telemetry_packet_t* pkt, const rsn_adc_stats_t* soil, const rsn_adc_stats_t* vbat, const rsn_adc_stats_t* ntc) {
    if (!pkt || !soil || !vbat || !ntc) {
        return;
    }
    proto_fill_header(&pkt->hdr, RSN_PKT_TELEMETRY);
    pkt->cycle           = g_rsn_status.cycle_count;
    pkt->ts_ms           = millis();
    pkt->batt_status     = g_rsn_telem.batt_status; // sera atualizado pelo chamador
    pkt->flags           = g_rsn_telem.flags;

    pkt->soil_mean_raw   = soil->mean;
    pkt->soil_median_raw = soil->median;
    pkt->soil_min_raw    = soil->min;
    pkt->soil_max_raw    = soil->max;
    pkt->soil_std_raw    = soil->stddev;

    pkt->vbat_mean_raw   = vbat->mean;
    pkt->vbat_median_raw = vbat->median;
    pkt->vbat_min_raw    = vbat->min;
    pkt->vbat_max_raw    = vbat->max;
    pkt->vbat_std_raw    = vbat->stddev;

    pkt->ntc_mean_raw    = ntc->mean;
    pkt->ntc_median_raw  = ntc->median;
    pkt->ntc_min_raw     = ntc->min;
    pkt->ntc_max_raw     = ntc->max;
    pkt->ntc_std_raw     = ntc->stddev;
}

bool proto_apply_config_from_packet(const rsn_config_packet_t* pkt) {
    if (!pkt) {
        return false;
    }
    // Se o handshake se perdeu, aproveite o header do CONFIG para fixar o node_id.
    if (pkt->hdr.node_id != RSN_NODE_ID_UNSET && pkt->hdr.node_id != g_rsn_status.node_id) {
        g_rsn_status.node_id = pkt->hdr.node_id;
        g_rsn_status.waiting_handshake = false;
    }
    // copia campo a campo para evitar depender de layout de cabecalho
    proto_fill_header(&g_rsn_config.hdr, RSN_PKT_CONFIG);
    g_rsn_config.sleep_time_s        = pkt->sleep_time_s;
    g_rsn_config.pwr_up_time_ms      = pkt->pwr_up_time_ms;
    g_rsn_config.settling_time_ms    = pkt->settling_time_ms;
    g_rsn_config.sampling_interval_ms= pkt->sampling_interval_ms;
    g_rsn_config.led_mode_default    = pkt->led_mode_default;
    g_rsn_config.batt_bucket         = pkt->batt_bucket;
    g_rsn_config.lostRx_limit        = pkt->lostRx_limit;
    g_rsn_config.debug_mode          = pkt->debug_mode;
    g_rsn_config.reset_flags         = pkt->reset_flags;
    return true;
}

bool proto_handle_handshake_packet(const rsn_handshake_packet_t* pkt) {
    if (!pkt) {
        return false;
    }
    g_rsn_status.node_id = pkt->hdr.node_id;
    return true;
}

bool proto_send_hello() {
    rsn_hello_packet_t pkt = {};
    proto_build_hello_packet(&pkt);
    return proto_send_packet(&pkt, sizeof(pkt));
}

bool proto_send_telemetry() {
    return proto_send_packet(&g_rsn_telem, sizeof(g_rsn_telem));
}

bool proto_send_config_ack(uint8_t status) {
    rsn_config_ack_packet_t pkt = {};
    proto_fill_header(&pkt.hdr, RSN_PKT_CONFIG_ACK);
    pkt.status = status;
    return proto_send_packet(&pkt, sizeof(pkt));
}

bool proto_last_send_ok() {
    return s_last_send_ok;
}

bool proto_try_receive_handshake(rsn_handshake_packet_t* hello_in) {
    if (!hello_in) {
        return false;
    }

    uint8_t local_buf[RSN_MAX_PACKET_SIZE] = {0};
    size_t copy_len = 0;
    bool available = false;

    noInterrupts();
    if (s_last_pkt.has_packet && s_last_pkt.type == RSN_PKT_HANDSHAKE) {
        copy_len = (s_last_pkt.len < static_cast<int>(sizeof(local_buf))) ? static_cast<size_t>(s_last_pkt.len) : sizeof(local_buf);
        memcpy(local_buf, g_rx_buffer, copy_len);
        s_last_pkt.has_packet = false;
        available = true;
    }
    interrupts();

    if (!available || copy_len < sizeof(rsn_handshake_packet_t)) {
        return false;
    }

    memcpy(hello_in, local_buf, sizeof(rsn_handshake_packet_t));
    return true;
}

bool proto_try_receive_config(rsn_config_packet_t* cfg_out) {
    if (!cfg_out) {
        return false;
    }

    uint8_t local_buf[RSN_MAX_PACKET_SIZE] = {0};
    size_t copy_len = 0;
    bool available = false;

    noInterrupts();
    if (s_last_pkt.has_packet && s_last_pkt.type == RSN_PKT_CONFIG) {
        copy_len = (s_last_pkt.len < static_cast<int>(sizeof(local_buf))) ? static_cast<size_t>(s_last_pkt.len) : sizeof(local_buf);
        memcpy(local_buf, g_rx_buffer, copy_len);
        s_last_pkt.has_packet = false;
        available = true;
    }
    interrupts();

    if (!available || copy_len < sizeof(rsn_config_packet_t)) {
        return false;
    }

    memcpy(cfg_out, local_buf, sizeof(rsn_config_packet_t));
    return true;
}
