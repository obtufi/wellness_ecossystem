#pragma once

#include "constants.h"

extern rsn_runtime_status_t   g_rsn_status;  // Flags e contadores persistentes.
extern rsn_mode_t             g_rsn_mode;    // Modo logico atual (running/pairing/debug).
extern rsn_state_t            g_rsn_state;   // Estado corrente da maquina principal.
extern rsn_config_packet_t    g_rsn_config;  // Ultima configuracao aplicada/recebida.
extern rsn_telemetry_packet_t g_rsn_telem;   // Buffer de telemetria do ciclo atual.
extern uint8_t                g_log_debug;   // Habilita logs seriais detalhados.

extern uint8_t g_rx_buffer[RSN_MAX_PACKET_SIZE]; // Buffer bruto para recepcao ESP-NOW.
extern uint8_t g_tx_buffer[RSN_MAX_PACKET_SIZE]; // Buffer bruto para envio ESP-NOW.
