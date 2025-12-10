#pragma once

void tgw_display_init();
void tgw_display_status(const char* line1, const char* line2);
// Mostra um resumo compacto: contadores globais + ultimo pacote do node.
void tgw_display_summary(uint32_t hello_count,
                         uint32_t telem_count,
                         uint8_t node_id,
                         uint32_t cycle,
                         uint16_t soil_mean_raw,
                         uint16_t vbat_mean_raw,
                         int8_t rssi);
