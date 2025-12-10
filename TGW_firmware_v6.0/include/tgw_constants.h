#pragma once

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// RSN packet types reused by o TGW
// ============================================================================

#define RSN_MAX_PACKET_SIZE   128u
#define RSN_HW_VERSION        1u
#define RSN_FW_VERSION        1u
#define RSN_NODE_ID_UNSET     0u

typedef enum : uint8_t {
    RSN_PKT_HELLO      = 0x01,
    RSN_PKT_HANDSHAKE  = 0x02,
    RSN_PKT_TELEMETRY  = 0x03,
    RSN_PKT_CONFIG     = 0x04,
    RSN_PKT_CONFIG_ACK = 0x05,
    RSN_PKT_DEBUG      = 0x06
} rsn_packet_type_t;

typedef enum : uint8_t {
    RSN_MODE_RUNNING = 0,
    RSN_MODE_PAIRING = 1,
    RSN_MODE_DEBUG   = 2
} rsn_mode_t;

typedef enum : uint8_t {
    RSN_TF_LOW_BATT     = (1u << 0),
    RSN_TF_LOST_RX      = (1u << 1),
    RSN_TF_DEBUG_MODE   = (1u << 2),
    RSN_TF_WATCHDOG_RST = (1u << 3),
    RSN_TF_BROWNOUT_RST = (1u << 4),
    RSN_TF_FIRST_BOOT   = (1u << 5)
} rsn_telem_flags_t;

typedef enum : uint8_t {
    RSN_BATT_LOW  = 0,
    RSN_BATT_MED  = 1,
    RSN_BATT_HIGH = 2
} rsn_batt_status_t;

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  node_id;
    uint8_t  mode;
    uint8_t  hw_version;
    uint8_t  fw_version;
} rsn_header_t;

typedef struct __attribute__((packed)) {
    rsn_header_t hdr;
    uint16_t     capabilities;
} rsn_hello_packet_t;

typedef struct __attribute__((packed)) {
    rsn_header_t hdr;
} rsn_handshake_packet_t;

typedef struct __attribute__((packed)) {
    rsn_header_t hdr;
    uint16_t sleep_time_s;
    uint16_t pwr_up_time_ms;
    uint16_t settling_time_ms;
    uint16_t sampling_interval_ms;
    uint8_t  led_mode_default;
    uint8_t  batt_bucket;
    uint8_t  lostRx_limit;
    uint8_t  debug_mode;
    uint8_t  reset_flags;
} rsn_config_packet_t;

typedef struct __attribute__((packed)) {
    rsn_header_t hdr;
    uint8_t      status;
} rsn_config_ack_packet_t;

typedef struct __attribute__((packed)) {
    rsn_header_t hdr;
    uint32_t cycle;
    uint32_t ts_ms;
    uint8_t  batt_status;
    uint8_t  flags;
    uint16_t soil_mean_raw;
    uint16_t soil_median_raw;
    uint16_t soil_min_raw;
    uint16_t soil_max_raw;
    uint16_t soil_std_raw;
    uint16_t vbat_mean_raw;
    uint16_t vbat_median_raw;
    uint16_t vbat_min_raw;
    uint16_t vbat_max_raw;
    uint16_t vbat_std_raw;
    uint16_t ntc_mean_raw;
    uint16_t ntc_median_raw;
    uint16_t ntc_min_raw;
    uint16_t ntc_max_raw;
    uint16_t ntc_std_raw;
    int8_t   last_rssi;
} rsn_telemetry_packet_t;

// ============================================================================
// Tipos especificos do TGW
// ============================================================================

#define TGW_MAX_NODES         8
#define TGW_TELEM_QUEUE_LEN   32

typedef struct {
    bool     in_use;
    uint8_t  node_id;
    int8_t   last_rssi;
    uint32_t last_seen_ms;
    uint8_t  mac[6];
} tgw_node_status_t;

typedef struct {
    uint8_t  node_id;
    int8_t   rssi;
    uint32_t local_ts_ms;
    rsn_telemetry_packet_t pkt;
} tgw_telem_item_t;

typedef enum {
    TGW_RX_FROM_RSN_NONE = 0,
    TGW_RX_FROM_RSN_HELLO,
    TGW_RX_FROM_RSN_TELEMETRY,
    TGW_RX_FROM_RSN_CONFIG_ACK,
    TGW_RX_FROM_RSN_DEBUG
} tgw_rx_type_t;
