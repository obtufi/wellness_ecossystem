// Lightweight persistence helpers (Preferences / NVS).
#pragma once

#include "constants.h"
#include "globals.h"

void persist_init();
void persist_load_status();
void persist_save_status();
void persist_load_config();
void persist_save_config();
