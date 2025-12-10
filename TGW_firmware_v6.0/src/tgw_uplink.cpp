#include <Arduino.h>
#include "tgw_uplink.h"

static void discard_serial_bytes(size_t len) {
    uint8_t sink[32];
    while (len > 0) {
        size_t chunk = (len < sizeof(sink)) ? len : sizeof(sink);
        size_t got = Serial.readBytes(sink, chunk);
        if (got == 0) break; // avoid blocking forever if sender stopped
        len -= got;
    }
}

void tgw_uplink_init() {
    // Serial deve ser inicializado no main/setup. Nada extra aqui.
}

bool tgw_uplink_is_connected() {
    // Em Serial consideramos sempre conectado; pode evoluir para handshake.
    return true;
}

bool tgw_uplink_send_frame(const uint8_t* data, size_t len) {
    if (!data || len == 0) return false;
    // Framing simples: [len LSB][len MSB][payload...]
    uint16_t l = (uint16_t)len;
    Serial.write((uint8_t)(l & 0xFF));
    Serial.write((uint8_t)((l >> 8) & 0xFF));
    size_t written = Serial.write(data, len);
    return written == len;
}

bool tgw_uplink_poll_frame(uint8_t* buf, size_t* len_in_out) {
    if (!buf || !len_in_out || Serial.available() < 2) return false;
    uint16_t len = (uint8_t)Serial.read();
    len |= ((uint16_t)Serial.read()) << 8;
    if (len == 0 || len > *len_in_out) {
        discard_serial_bytes(len);
        return false;
    }
    size_t got = Serial.readBytes(buf, len);
    if (got != len) {
        if (len > got) discard_serial_bytes(len - got);
        return false;
    }
    *len_in_out = len;
    return true;
}
