#include <Arduino.h>
#include <Preferences.h>
#include <string.h>

#include "tgw_store.h"

static Preferences s_prefs_cfg;

static tgw_telem_item_t s_telem_fifo[TGW_TELEM_QUEUE_LEN];
static uint8_t s_telem_head = 0;
static uint8_t s_telem_tail = 0;

void tgw_store_init() {
    s_prefs_cfg.begin("tgw_cfg", false);
}

static String key_for_node(uint8_t node_id) {
    char k[8];
    snprintf(k, sizeof(k), "cfg%02u", node_id);
    return String(k);
}

bool tgw_store_load_node_config(uint8_t node_id, rsn_config_packet_t* cfg) {
    if (!cfg) return false;
    String key = key_for_node(node_id);
    size_t len = s_prefs_cfg.getBytesLength(key.c_str());
    if (len != sizeof(rsn_config_packet_t)) return false;
    return s_prefs_cfg.getBytes(key.c_str(), cfg, len) == len;
}

bool tgw_store_save_node_config(uint8_t node_id, const rsn_config_packet_t* cfg) {
    if (!cfg) return false;
    String key = key_for_node(node_id);
    return s_prefs_cfg.putBytes(key.c_str(), cfg, sizeof(rsn_config_packet_t)) == sizeof(rsn_config_packet_t);
}

static bool telem_fifo_push(const tgw_telem_item_t* item) {
    uint8_t next = (s_telem_head + 1u) % (uint8_t)TGW_TELEM_QUEUE_LEN;
    if (next == s_telem_tail) {
        Serial.println("[STORE] Telemetry queue full; dropping new entry");
        return false; // fila cheia
    }
    s_telem_fifo[s_telem_head] = *item;
    s_telem_head = next;
    return true;
}

static bool telem_fifo_pop(tgw_telem_item_t* item_out) {
    if (s_telem_head == s_telem_tail) return false;
    *item_out = s_telem_fifo[s_telem_tail];
    s_telem_tail = (s_telem_tail + 1u) % (uint8_t)TGW_TELEM_QUEUE_LEN;
    return true;
}

bool tgw_store_push_telem(const tgw_telem_item_t* item) {
    if (!item) return false;
    return telem_fifo_push(item);
}

bool tgw_store_pop_telem(tgw_telem_item_t* item_out) {
    if (!item_out) return false;
    return telem_fifo_pop(item_out);
}

bool tgw_store_has_pending_telem() {
    return s_telem_head != s_telem_tail;
}
