#include <Arduino.h>
#include <math.h>
#include "hw_helpers.h"

void hw_init_pins() {
    pinMode(PIN_SENSE_EN, OUTPUT);
    pinMode(PIN_VBAT_GND_EN, OUTPUT);
    pinMode(PIN_NTC_GND_EN, OUTPUT);

    pinMode(PIN_LED_RED, OUTPUT);
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_BLUE, OUTPUT);

    pinMode(PIN_SOIL_ADC, INPUT);
    pinMode(PIN_VBAT_SENSE, INPUT);
    pinMode(PIN_NTC_SENSE, INPUT);

    hw_disable_sensors();
    hw_led_all_off();
}

void hw_init_adc() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);
    analogSetPinAttenuation(PIN_VBAT_SENSE, ADC_11db);
    analogSetPinAttenuation(PIN_NTC_SENSE, ADC_11db);
    analogSetPinAttenuation(PIN_SOIL_ADC, ADC_11db);
}

void hw_enable_sensors() {
    VBAT_GND_ON();
    NTC_GND_ON();
    SOIL_ON();
}

void hw_disable_sensors() {
    VBAT_GND_OFF();
    NTC_GND_OFF();
    SOIL_OFF();
}

static uint16_t compute_median_u16(uint16_t* buf, uint8_t count) {
    // Ordenacao simples para poucos elementos.
    for (uint8_t i = 0; i < count; ++i) {
        for (uint8_t j = i + 1; j < count; ++j) {
            if (buf[j] < buf[i]) {
                const uint16_t tmp = buf[i];
                buf[i] = buf[j];
                buf[j] = tmp;
            }
        }
    }
    if ((count & 1u) == 0u) {
        const uint8_t mid = count / 2;
        return static_cast<uint16_t>((buf[mid - 1] + buf[mid]) / 2);
    }
    return buf[count / 2];
}

static rsn_adc_stats_t compute_stats(uint16_t* samples, uint8_t count) {
    rsn_adc_stats_t s = {0, 0, UINT16_MAX, 0, 0, count};
    if (count == 0) {
        s.min = 0;
        s.max = 0;
        s.mean = 0;
        s.median = 0;
        s.stddev = 0;
        return s;
    }

    uint32_t acc = 0;
    for (uint8_t i = 0; i < count; ++i) {
        const uint16_t v = samples[i];
        acc += v;
        s.min = min(s.min, v);
        s.max = max(s.max, v);
    }
    s.mean = static_cast<uint16_t>(acc / count);
    s.median = compute_median_u16(samples, count);

    uint32_t var_acc = 0;
    for (uint8_t i = 0; i < count; ++i) {
        const int32_t diff = static_cast<int32_t>(samples[i]) - s.mean;
        var_acc += static_cast<uint32_t>(diff * diff);
    }
    const uint32_t variance = var_acc / count;
    s.stddev = static_cast<uint16_t>(sqrtf(static_cast<float>(variance)));
    return s;
}

static uint8_t clamp_num_samples(uint8_t num_samples) {
    if (num_samples == 0 || num_samples > RSN_MAX_ADC_SAMPLES) {
        return RSN_DEFAULT_NUM_SAMPLES;
    }
    return num_samples;
}

rsn_adc_stats_t hw_adc_read_soil_burst(uint8_t num_samples, uint16_t settling_time_ms, uint16_t sample_interval_ms) {
    uint16_t samples[RSN_MAX_ADC_SAMPLES] = {0};
    const uint8_t n = clamp_num_samples(num_samples);

    SOIL_ON();
    delay(settling_time_ms);
    (void)analogRead(PIN_SOIL_ADC); // descarta primeira medida do burst

    for (uint8_t i = 0; i < n; ++i) {
        samples[i] = analogRead(PIN_SOIL_ADC);
        if (i + 1u < n) {
            delay(sample_interval_ms);
        }
    }

    SOIL_OFF();
    return compute_stats(samples, n);
}

rsn_adc_stats_t hw_adc_read_vbat_burst(uint8_t num_samples, uint16_t settling_time_ms, uint16_t sample_interval_ms) {
    uint16_t samples[RSN_MAX_ADC_SAMPLES] = {0};
    const uint8_t n = clamp_num_samples(num_samples);

    VBAT_GND_ON();
    delay(settling_time_ms);
    (void)analogRead(PIN_VBAT_SENSE); // descarta primeira medida

    for (uint8_t i = 0; i < n; ++i) {
        samples[i] = analogRead(PIN_VBAT_SENSE);
        if (i + 1u < n) {
            delay(sample_interval_ms);
        }
    }

    VBAT_GND_OFF();
    return compute_stats(samples, n);
}

rsn_adc_stats_t hw_adc_read_ntc_burst(uint8_t num_samples, uint16_t settling_time_ms, uint16_t sample_interval_ms) {
    uint16_t samples[RSN_MAX_ADC_SAMPLES] = {0};
    const uint8_t n = clamp_num_samples(num_samples);

    NTC_GND_ON();
    delay(settling_time_ms);
    (void)analogRead(PIN_NTC_SENSE); // descarta primeira medida

    for (uint8_t i = 0; i < n; ++i) {
        samples[i] = analogRead(PIN_NTC_SENSE);
        if (i + 1u < n) {
            delay(sample_interval_ms);
        }
    }

    NTC_GND_OFF();
    return compute_stats(samples, n);
}

void hw_led_all_off() {
    LED_RED_OFF();
    LED_GREEN_OFF();
    LED_BLUE_OFF();
}

void hw_led_running_pattern(rsn_batt_status_t batt_status, bool tx_ok, bool low_batt_flag) {
    hw_led_all_off();

    if (low_batt_flag || batt_status == RSN_BATT_LOW) {
        LED_RED_ON();
    } else if (!tx_ok) {
        LED_BLUE_ON();
    } else {
        LED_GREEN_ON();
    }

    delay(60);
    hw_led_all_off();
}

void hw_led_pairing_pattern(bool waiting_config) {
    hw_led_all_off();
    if (waiting_config) {
        LED_BLUE_ON();
        LED_GREEN_ON(); // cyan to signal waiting config
    } else {
        LED_BLUE_ON();
    }
    delay(80);
    hw_led_all_off();
}

void hw_led_lost_rx_pattern() {
    hw_led_all_off();
    LED_RED_ON();
    delay(80);
    hw_led_all_off();
    delay(80);
}

void hw_led_debug_pattern() {
    hw_led_all_off();
    LED_GREEN_ON();
    LED_BLUE_ON();
    delay(40);
    hw_led_all_off();
}
