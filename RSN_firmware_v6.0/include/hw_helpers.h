// Hardware helper functions for GPIO, ADC and LED handling.
#pragma once

#include "constants.h"

void hw_init_pins();
void hw_init_adc();

void hw_enable_sensors();
void hw_disable_sensors();

rsn_adc_stats_t hw_adc_read_soil_burst(uint8_t num_samples, uint16_t settling_time_ms, uint16_t sample_interval_ms);
rsn_adc_stats_t hw_adc_read_vbat_burst(uint8_t num_samples, uint16_t settling_time_ms, uint16_t sample_interval_ms);
rsn_adc_stats_t hw_adc_read_ntc_burst(uint8_t num_samples, uint16_t settling_time_ms, uint16_t sample_interval_ms);

void hw_led_all_off();
void hw_led_running_pattern(rsn_batt_status_t batt_status, bool tx_ok, bool low_batt_flag);
void hw_led_pairing_pattern(bool waiting_config);
void hw_led_lost_rx_pattern();
void hw_led_debug_pattern();
