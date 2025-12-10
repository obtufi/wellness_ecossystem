# Orientação de uso do RSN-Home v5.0

Instruções práticas para instalar, parear e operar o nó RSN-Home v5.0 com ESP32 e ESP-NOW.

## 1. Preparação
1) Alimente o RSN (bateria conectada) e garanta antena adequada.
2) Certifique-se de que o TGW está ligado e com ESP-NOW habilitado para receber Hello/telemetria.
3) Se precisar gravar firmware: `pio run -e rsn-node -t upload` e, opcionalmente, abra o serial: `pio device monitor -b 115200`.

## 2. Pareamento
1) Ao ligar sem config válida, o RSN entra em PAIRING_HELLO e envia Hello por broadcast.
2) O TGW deve responder com um pacote de handshake (define `node_id`).
3) Em seguida, o TGW envia a config (inclui `batt_bucket`, tempos, limites, etc.).
4) O RSN salva a config, envia ACK e passa para RUNNING. Se handshake/config não chegarem, ele entra em sleep e tenta de novo no próximo ciclo.

## 3. Operação normal
1) Cada ciclo faz:
   - `pwr_up_time_ms` de espera inicial.
   - Bursts de ADC para solo, VBAT e NTC (stats brutas: mean/median/min/max/std).
   - Monta telemetria com esses valores brutos e envia via ESP-NOW.
2) O RSN não classifica VBAT internamente; usa `batt_bucket` recebido do TGW para sinalizar low/med/high.
3) Após TX/RX, decide dormir; se low_batt_flag ou lost_rx_flag, aumenta o tempo de sono.

## 4. Modo debug
- Se `debug_mode` vier na config, o RSN entra em DEBUG_LOOP: medições e TX contínuos, sem deep sleep, com LED de debug.
- Saia do debug reenviando config com `debug_mode = 0` ou resetando o nó com config não debug.

## 5. LEDs (ânodo comum, LOW acende)
- Pairing: azul/ciano piscando enquanto espera handshake/config.
- Running: verde em sucesso, azul em falha de TX, vermelho se low_batt_flag.
- Lost RX: vermelho intermitente.
- Debug: azul+verde rápido.

## 6. Dicas de confiabilidade
- Sempre forneça `batt_bucket` na config; o nó não calcula VBAT→bateria.
- Ajuste `pwr_up_time_ms`, `settling_time_ms` e `sampling_interval_ms` conforme hardware/sensor.
- `lostRx_limit` controla quando o nó abandona a config atual e volta para pairing.
- Evite loops longos acordado: o RSN tem timeouts em pairing e espera de config; se não chegar nada, ele dorme e tenta novamente.
