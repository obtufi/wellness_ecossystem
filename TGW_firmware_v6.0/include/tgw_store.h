#pragma once

#include "tgw_constants.h"

void tgw_store_init();

bool tgw_store_load_node_config(uint8_t node_id, rsn_config_packet_t* cfg);
bool tgw_store_save_node_config(uint8_t node_id, const rsn_config_packet_t* cfg);

bool tgw_store_push_telem(const tgw_telem_item_t* item);
bool tgw_store_pop_telem(tgw_telem_item_t* item_out);
bool tgw_store_has_pending_telem();
