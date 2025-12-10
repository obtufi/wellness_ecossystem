#include "globals.h"

// Estado persistente e contadores principais usados pela maquina de estados.
rsn_runtime_status_t g_rsn_status = {
    RSN_NODE_ID_UNSET, // node_id
    false,             // config_valid
    false,             // debug_mode
    false,             // low_batt_flag
    false,             // lost_rx_flag
    false,             // waiting_handshake
    false,             // waiting_config
    0,                 // last_reset_cause
    0,                 // rx_failed
    0                  // cycle_count
};

// Modo logico ativo (running/pairing/debug).
rsn_mode_t  g_rsn_mode  = RSN_MODE_RUNNING;
// Estado corrente da maquina principal.
rsn_state_t g_rsn_state = RSN_STATE_BOOT;
// Flag de logs seriais detalhados.
uint8_t g_log_debug = RSN_DEFAULT_LOG_DEBUG;

// Ultima configuracao aplicada/recebida.
rsn_config_packet_t g_rsn_config = {
    {RSN_PKT_CONFIG, RSN_NODE_ID_UNSET, RSN_MODE_RUNNING, RSN_HW_VERSION, RSN_FW_VERSION},
    RSN_DEFAULT_SLEEP_S,     // sleep_time_s
    RSN_DEFAULT_PWR_UP_MS,   // pwr_up_time_ms
    RSN_DEFAULT_SETTLE_MS,   // settling_time_ms
    RSN_DEFAULT_SAMPLE_MS,   // sampling_interval_ms
    1,                       // led_mode_default
    0,                       // batt_bucket
    RSN_DEFAULT_LOSTRX_LIMIT,// lostRx_limit
    0,                       // debug_mode
    0                        // reset_flags
};

// Buffer de telemetria do ciclo atual.
rsn_telemetry_packet_t g_rsn_telem = {0};

// Buffers brutos de RX/TX para ESP-NOW.
uint8_t g_rx_buffer[RSN_MAX_PACKET_SIZE];
uint8_t g_tx_buffer[RSN_MAX_PACKET_SIZE];
