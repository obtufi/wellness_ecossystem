#include <Arduino.h>

#include "tgw_logic.h"

void setup() {
    Serial.begin(115200);
    delay(100);
    tgw_logic_init();
}

void loop() {
    tgw_logic_step();
    delay(10);
}
