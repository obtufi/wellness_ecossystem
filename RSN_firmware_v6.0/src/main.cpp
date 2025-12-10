#include <Arduino.h>
#include "rsn_core.h"

void setup() {
    rsn_init();
}

void loop() {
    rsn_step();
}