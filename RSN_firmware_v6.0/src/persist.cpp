#include <Arduino.h>
#include <Preferences.h>

#include "persist.h"

static Preferences s_prefs_status;
static Preferences s_prefs_config;

// Garantir que os valores carregados fiquem dentro de limites seguros.
static void sanitize_config() {
    if (g_rsn_config.sleep_time_s == 0) {
        g_rsn_config.sleep_time_s = RSN_DEFAULT_SLEEP_S;
    }
    if (g_rsn_config.sleep_time_s > 36000u) {
        g_rsn_config.sleep_time_s = 36000u;
    }
    if (g_rsn_config.lostRx_limit == 0) {
        g_rsn_config.lostRx_limit = RSN_DEFAULT_LOSTRX_LIMIT;
    }
    if (g_rsn_config.pwr_up_time_ms == 0) {
        g_rsn_config.pwr_up_time_ms = RSN_DEFAULT_PWR_UP_MS;
    }
    if (g_rsn_config.settling_time_ms == 0) {
        g_rsn_config.settling_time_ms = RSN_DEFAULT_SETTLE_MS;
    }
    if (g_rsn_config.sampling_interval_ms == 0) {
        g_rsn_config.sampling_interval_ms = RSN_DEFAULT_SAMPLE_MS;
    }
}

void persist_init() {
    s_prefs_status.begin("rsn_status", false);
    s_prefs_config.begin("rsn_config", false);
}

void persist_load_status() {
    g_rsn_status.node_id          = s_prefs_status.getUChar("node_id", g_rsn_status.node_id);
    g_rsn_status.config_valid     = s_prefs_status.getBool("cfg_valid", g_rsn_status.config_valid);
    g_rsn_status.debug_mode       = s_prefs_status.getBool("debug_mode", g_rsn_status.debug_mode);
    g_rsn_status.low_batt_flag    = s_prefs_status.getBool("low_batt", g_rsn_status.low_batt_flag);
    g_rsn_status.lost_rx_flag     = s_prefs_status.getBool("lost_rx", g_rsn_status.lost_rx_flag);
    g_rsn_status.waiting_handshake= s_prefs_status.getBool("wait_hs", g_rsn_status.waiting_handshake);
    g_rsn_status.waiting_config   = s_prefs_status.getBool("wait_cfg", g_rsn_status.waiting_config);
    g_rsn_status.last_reset_cause = s_prefs_status.getUChar("rst_cause", g_rsn_status.last_reset_cause);
    g_rsn_status.rx_failed        = s_prefs_status.getUInt("rx_failed", g_rsn_status.rx_failed);
    g_rsn_status.cycle_count      = s_prefs_status.getUInt("cycle_cnt", g_rsn_status.cycle_count);
}

void persist_save_status() {
    s_prefs_status.putUChar("node_id", g_rsn_status.node_id);
    s_prefs_status.putBool("cfg_valid", g_rsn_status.config_valid);
    s_prefs_status.putBool("debug_mode", g_rsn_status.debug_mode);
    s_prefs_status.putBool("low_batt", g_rsn_status.low_batt_flag);
    s_prefs_status.putBool("lost_rx", g_rsn_status.lost_rx_flag);
    s_prefs_status.putBool("wait_hs", g_rsn_status.waiting_handshake);
    s_prefs_status.putBool("wait_cfg", g_rsn_status.waiting_config);
    s_prefs_status.putUChar("rst_cause", g_rsn_status.last_reset_cause);
    s_prefs_status.putUInt("rx_failed", g_rsn_status.rx_failed);
    s_prefs_status.putUInt("cycle_cnt", g_rsn_status.cycle_count);
}

void persist_load_config() {
    g_rsn_config.sleep_time_s       = s_prefs_config.getUShort("sleep_s", g_rsn_config.sleep_time_s);
    g_rsn_config.pwr_up_time_ms     = s_prefs_config.getUShort("pwr_ms", g_rsn_config.pwr_up_time_ms);
    g_rsn_config.settling_time_ms   = s_prefs_config.getUShort("settle_ms", g_rsn_config.settling_time_ms);
    g_rsn_config.sampling_interval_ms = s_prefs_config.getUShort("samp_ms", g_rsn_config.sampling_interval_ms);
    g_rsn_config.led_mode_default   = s_prefs_config.getUChar("led_mode", g_rsn_config.led_mode_default);
    g_rsn_config.batt_bucket        = s_prefs_config.getUChar("batt_bucket", g_rsn_config.batt_bucket);
    g_rsn_config.lostRx_limit       = s_prefs_config.getUChar("lost_rx_lim", g_rsn_config.lostRx_limit);
    g_rsn_config.debug_mode         = s_prefs_config.getUChar("dbg_mode", g_rsn_config.debug_mode);
    g_rsn_config.reset_flags        = s_prefs_config.getUChar("rst_flags", g_rsn_config.reset_flags);
    sanitize_config();
}

void persist_save_config() {
    s_prefs_config.putUShort("sleep_s", g_rsn_config.sleep_time_s);
    s_prefs_config.putUShort("pwr_ms", g_rsn_config.pwr_up_time_ms);
    s_prefs_config.putUShort("settle_ms", g_rsn_config.settling_time_ms);
    s_prefs_config.putUShort("samp_ms", g_rsn_config.sampling_interval_ms);
    s_prefs_config.putUChar("led_mode", g_rsn_config.led_mode_default);
    s_prefs_config.putUChar("batt_bucket", g_rsn_config.batt_bucket);
    s_prefs_config.putUChar("lost_rx_lim", g_rsn_config.lostRx_limit);
    s_prefs_config.putUChar("dbg_mode", g_rsn_config.debug_mode);
    s_prefs_config.putUChar("rst_flags", g_rsn_config.reset_flags);
}
