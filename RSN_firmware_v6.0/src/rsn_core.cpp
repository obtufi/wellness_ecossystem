#include <Arduino.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <stdarg.h>

#include "rsn_core.h"
#include "hw_helpers.h"
#include "proto_helpers.h"
#include "persist.h"

static rsn_state_t s_prev_state      = RSN_STATE_BOOT;
static uint32_t    s_state_enter_ms  = 0;
static bool        s_last_tx_ok      = false;
static uint8_t     s_pairing_attempts = 0; // contador de hellos no ciclo atual

static void reset_state_timer() {
    s_prev_state = g_rsn_state;
    s_state_enter_ms = millis();
}

static void log_msg(const char* fmt, ...) {
    if (!g_log_debug) {
        return;
    }
    char buf[160];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    Serial.println(buf);
}

static void populate_telem_flags() {
    uint8_t flags = 0;
    if (g_rsn_status.low_batt_flag)  flags |= RSN_TF_LOW_BATT;
    if (g_rsn_status.lost_rx_flag)   flags |= RSN_TF_LOST_RX;
    if (g_rsn_status.debug_mode)     flags |= RSN_TF_DEBUG_MODE;

    const uint8_t cause = g_rsn_status.last_reset_cause;
    if (cause == ESP_RST_WDT) {
        flags |= RSN_TF_WATCHDOG_RST;
    } else if (cause == ESP_RST_BROWNOUT) {
        flags |= RSN_TF_BROWNOUT_RST;
    }

    g_rsn_telem.flags = flags;
}

static void perform_measurement() {
    hw_disable_sensors(); // garante que tudo inicia desligado.

    const uint16_t pwr_up_ms = g_rsn_config.pwr_up_time_ms ? g_rsn_config.pwr_up_time_ms : RSN_DEFAULT_PWR_UP_MS;
    const uint16_t settle_ms = g_rsn_config.settling_time_ms ? g_rsn_config.settling_time_ms : RSN_DEFAULT_SETTLE_MS;
    const uint16_t sample_interval_ms = g_rsn_config.sampling_interval_ms ? g_rsn_config.sampling_interval_ms : RSN_DEFAULT_SAMPLE_MS;
    const uint8_t  num_samples = RSN_DEFAULT_NUM_SAMPLES;

    delay(pwr_up_ms);

    const rsn_adc_stats_t soil_stats = hw_adc_read_soil_burst(num_samples, settle_ms, sample_interval_ms);
    delay(settle_ms); // respiro entre sensores
    const rsn_adc_stats_t vbat_stats = hw_adc_read_vbat_burst(num_samples, settle_ms, sample_interval_ms);
    delay(settle_ms); // respiro entre sensores
    const rsn_adc_stats_t ntc_stats  = hw_adc_read_ntc_burst(num_samples, settle_ms, sample_interval_ms);

    g_rsn_telem.batt_status = g_rsn_config.batt_bucket; // status vindo do TGW/config
    g_rsn_status.low_batt_flag = (g_rsn_config.batt_bucket == RSN_BATT_LOW);
    populate_telem_flags();

    g_rsn_telem.last_rssi   = 0x7F;
    proto_build_telemetry_packet(&g_rsn_telem, &soil_stats, &vbat_stats, &ntc_stats);

    ++g_rsn_status.cycle_count;

    hw_disable_sensors();
}

static void enter_sleep(uint32_t sleep_seconds) {
    hw_disable_sensors();
    hw_led_all_off();

    persist_save_status();

    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(sleep_seconds) * 1000000ULL);
    esp_deep_sleep_start();
}

void rsn_init() {
    Serial.begin(115200);
    delay(50);

    persist_init();
    persist_load_status();
    persist_load_config();

    g_log_debug = (g_rsn_status.debug_mode || g_rsn_config.debug_mode || g_log_debug);

    g_rsn_state = RSN_STATE_BOOT;
    g_rsn_mode  = g_rsn_status.debug_mode ? RSN_MODE_DEBUG : RSN_MODE_RUNNING;

    hw_init_pins();
    hw_init_adc();
    proto_init();

    g_rsn_status.last_reset_cause = static_cast<uint8_t>(esp_reset_reason());
    reset_state_timer();
    s_pairing_attempts = 0;

    log_msg("[INIT] node_id=%u config_valid=%d debug_mode=%d", g_rsn_status.node_id, g_rsn_status.config_valid, g_rsn_status.debug_mode);
}

static rsn_state_t rsn_state_boot() {
    log_msg("[BOOT] entering BOOT");
    return RSN_STATE_CHECK_CONFIG;
}

static rsn_state_t rsn_state_check_config() {
    if (g_rsn_status.config_valid && g_rsn_status.node_id != RSN_NODE_ID_UNSET) {
        g_rsn_mode = g_rsn_status.debug_mode ? RSN_MODE_DEBUG : RSN_MODE_RUNNING;
        log_msg("[CHECK_CONFIG] valid config; mode=%u next=%s", g_rsn_mode, g_rsn_status.debug_mode ? "DEBUG_LOOP" : "RUNNING_MEASURE");
        return g_rsn_status.debug_mode ? RSN_STATE_DEBUG_LOOP : RSN_STATE_RUNNING_MEASURE;
    }
    g_rsn_mode = RSN_MODE_PAIRING;
    g_rsn_status.waiting_handshake = true;
    g_rsn_status.waiting_config = true;
    log_msg("[CHECK_CONFIG] config invalid -> pairing");
    return RSN_STATE_PAIRING_HELLO;
}

static rsn_state_t rsn_state_pairing_hello() {
    ++s_pairing_attempts;
    proto_send_hello();
    log_msg("[PAIRING_HELLO] sent hello broadcast");
    hw_led_pairing_pattern(true);
    return RSN_STATE_PAIRING_WAIT_HANDSHAKE;
}

static rsn_state_t rsn_state_pairing_wait_handshake() {
    rsn_handshake_packet_t hs = {};
    if (proto_try_receive_handshake(&hs)) {
        proto_handle_handshake_packet(&hs);
        g_rsn_status.waiting_handshake = false;
        g_rsn_status.waiting_config = true;
        g_rsn_status.rx_failed = 0;
        s_pairing_attempts = 0;
        s_last_tx_ok = true;
        persist_save_status();
        log_msg("[PAIRING_WAIT] handshake ok; node_id=%u", g_rsn_status.node_id);
        return RSN_STATE_RUNNING_RX;
    }
    if (millis() - s_state_enter_ms > 4000) {
        if (s_pairing_attempts < 3) {
            log_msg("[PAIRING_WAIT] timeout, retry hello (%u)", s_pairing_attempts);
            return RSN_STATE_PAIRING_HELLO;
        }
        log_msg("[PAIRING_WAIT] timeout -> sleep");
        return RSN_STATE_SLEEP; // evita ficar acordado indefinidamente
    }
    hw_led_pairing_pattern(true);
    return RSN_STATE_PAIRING_WAIT_HANDSHAKE;
}

static rsn_state_t rsn_state_running_measure() {
    perform_measurement();
    log_msg("[RUN_MEASURE] soil mean=%u median=%u; vbat mean=%u; ntc mean=%u",
            g_rsn_telem.soil_mean_raw, g_rsn_telem.soil_median_raw,
            g_rsn_telem.vbat_mean_raw, g_rsn_telem.ntc_mean_raw);
    return RSN_STATE_RUNNING_TX;
}

static rsn_state_t rsn_state_running_tx() {
    s_last_tx_ok = proto_send_telemetry();
    log_msg("[RUN_TX] telem sent ok=%d", s_last_tx_ok ? 1 : 0);
    hw_led_running_pattern(static_cast<rsn_batt_status_t>(g_rsn_telem.batt_status), s_last_tx_ok, g_rsn_status.low_batt_flag);
    return RSN_STATE_RUNNING_RX;
}

static rsn_state_t rsn_state_running_rx() {
    s_last_tx_ok = proto_last_send_ok();

    rsn_config_packet_t cfg = {};
    if (proto_try_receive_config(&cfg) && proto_apply_config_from_packet(&cfg)) {
        log_msg("[RUN_RX] config received");
        return RSN_STATE_RUNNING_CONFIG;
    }
    if (g_rsn_status.waiting_config && (millis() - s_state_enter_ms > 4000)) {
        log_msg("[RUN_RX] waiting config timeout -> sleep");
        return RSN_STATE_SLEEP; // economiza bateria aguardando config em outro ciclo
    }
    if (!s_last_tx_ok) {
        ++g_rsn_status.rx_failed;
        log_msg("[RUN_RX] tx fail count=%u", g_rsn_status.rx_failed);
        if (g_rsn_config.lostRx_limit > 0 && g_rsn_status.rx_failed >= g_rsn_config.lostRx_limit) {
            return RSN_STATE_LOST_RX;
        }
        return g_rsn_status.low_batt_flag ? RSN_STATE_LOW_BATT : RSN_STATE_SLEEP;
    }

    g_rsn_status.rx_failed = 0;
    g_rsn_status.lost_rx_flag = false;
    return g_rsn_status.low_batt_flag ? RSN_STATE_LOW_BATT : RSN_STATE_SLEEP;
}

static rsn_state_t rsn_state_running_config() {
    g_rsn_status.config_valid   = true;
    g_rsn_status.waiting_config = false;
    g_rsn_status.debug_mode     = (g_rsn_config.debug_mode != 0);
    g_rsn_status.low_batt_flag  = (g_rsn_config.batt_bucket == RSN_BATT_LOW);
    g_rsn_mode = g_rsn_status.debug_mode ? RSN_MODE_DEBUG : RSN_MODE_RUNNING;
    g_log_debug = (g_rsn_status.debug_mode || g_rsn_config.debug_mode || g_log_debug);
    log_msg("[RUN_CONFIG] applied config: sleep_s=%u settle_ms=%u samples=%u debug=%u",
            g_rsn_config.sleep_time_s, g_rsn_config.settling_time_ms, RSN_DEFAULT_NUM_SAMPLES, g_rsn_config.debug_mode);

    persist_save_config();
    persist_save_status();

    proto_send_config_ack(0);
    return g_rsn_status.debug_mode ? RSN_STATE_DEBUG_LOOP : RSN_STATE_RUNNING_MEASURE;
}

static rsn_state_t rsn_state_lost_rx() {
    g_rsn_status.lost_rx_flag = true;
    log_msg("[LOST_RX] rx_failed=%u limit=%u", g_rsn_status.rx_failed, g_rsn_config.lostRx_limit);
    hw_led_lost_rx_pattern();

    if (g_rsn_config.lostRx_limit > 0 && g_rsn_status.rx_failed >= g_rsn_config.lostRx_limit) {
        g_rsn_status.config_valid = false;
        g_rsn_status.node_id = RSN_NODE_ID_UNSET;
        g_rsn_status.waiting_handshake = true;
        g_rsn_status.rx_failed = 0;
        log_msg("[LOST_RX] returning to pairing");
        return RSN_STATE_PAIRING_HELLO;
    }
    return RSN_STATE_SLEEP;
}

static rsn_state_t rsn_state_low_batt() {
    log_msg("[LOW_BATT] entering low battery state");
    hw_led_running_pattern(RSN_BATT_LOW, s_last_tx_ok, true);
    return RSN_STATE_SLEEP;
}

static rsn_state_t rsn_state_debug_loop() {
    const uint32_t now = millis();
    const uint16_t interval = g_rsn_config.sampling_interval_ms ? g_rsn_config.sampling_interval_ms : RSN_DEFAULT_SAMPLE_MS;
    if (now - s_state_enter_ms >= interval) {
        perform_measurement();
        proto_send_telemetry();
        s_state_enter_ms = now;
        log_msg("[DEBUG_LOOP] measurement + telemetry sent");
    }
    hw_led_debug_pattern();
    return RSN_STATE_DEBUG_LOOP;
}

static rsn_state_t rsn_state_sleep() {
    uint32_t sleep_time = g_rsn_config.sleep_time_s ? g_rsn_config.sleep_time_s : RSN_DEFAULT_SLEEP_S;
    if (g_rsn_status.low_batt_flag) {
        sleep_time = (sleep_time * 13u) / 10u; // +30% on low battery
    }
    if (g_rsn_status.lost_rx_flag) {
        sleep_time += sleep_time / 2; // extend sleep if we are trying to recover
    }
    log_msg("[SLEEP] sleep_time=%u low_batt=%d lost_rx=%d", sleep_time, g_rsn_status.low_batt_flag, g_rsn_status.lost_rx_flag);
    enter_sleep(sleep_time);
    return RSN_STATE_SLEEP; // not reached; deep sleep will reboot.
}

void rsn_step() {
    if (g_rsn_state != s_prev_state) {
        reset_state_timer();
    }

    rsn_state_t next = g_rsn_state;
    switch (g_rsn_state) {
        case RSN_STATE_BOOT:               next = rsn_state_boot(); break;
        case RSN_STATE_CHECK_CONFIG:       next = rsn_state_check_config(); break;
        case RSN_STATE_PAIRING_HELLO:      next = rsn_state_pairing_hello(); break;
        case RSN_STATE_PAIRING_WAIT_HANDSHAKE: next = rsn_state_pairing_wait_handshake(); break;
        case RSN_STATE_RUNNING_MEASURE:    next = rsn_state_running_measure(); break;
        case RSN_STATE_RUNNING_TX:         next = rsn_state_running_tx(); break;
        case RSN_STATE_RUNNING_RX:         next = rsn_state_running_rx(); break;
        case RSN_STATE_RUNNING_CONFIG:     next = rsn_state_running_config(); break;
        case RSN_STATE_LOST_RX:            next = rsn_state_lost_rx(); break;
        case RSN_STATE_LOW_BATT:           next = rsn_state_low_batt(); break;
        case RSN_STATE_DEBUG_LOOP:         next = rsn_state_debug_loop(); break;
        case RSN_STATE_SLEEP:              next = rsn_state_sleep(); break;
    }
    g_rsn_state = next;
}
