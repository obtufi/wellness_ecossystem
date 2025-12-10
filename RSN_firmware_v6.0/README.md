# RSN-Home v5.0 (ESP32 + Arduino)

Firmware procedural para o nodo de sensor remoto RSN-Home v5.0 (ESP32 + ESP-NOW). A organização é simples e focada em depuração: constantes/tipos, globais, helpers de hardware, helpers de protocolo, persistência e máquina de estados.

## Estrutura de pastas
- `include/constants.h` — pinos, macros de hardware, enums de modos/estados, tipos de pacotes e estatísticas de ADC.
- `include/globals.h` / `src/globals.cpp` — variáveis globais únicas (status runtime, config corrente, telemetria e buffers ESP-NOW).
- `include/hw_helpers.h` / `src/hw_helpers.cpp` — inicialização de pinos/ADC, bursts de ADC (solo/VBAT/NTC) com stats brutas, padrões de LED.
- `include/proto_helpers.h` / `src/proto_helpers.cpp` — montagem/envio/recepção de pacotes ESP-NOW (hello, telemetria, config, handshake, ACK).
- `include/persist.h` / `src/persist.cpp` — persistência leve via Preferences (status e config).
- `include/rsn_core.h` / `src/rsn_core.cpp` — máquina de estados principal (boot, pairing, running, debug, lost_rx, low_batt, sleep).
- `src/main.cpp` — apenas chama `rsn_init()` e `rsn_step()`.

## Máquina de estados (resumo)
- **BOOT → CHECK_CONFIG**: decide entre RUNNING/DEBUG ou PAIRING.
- **PAIRING_HELLO / PAIRING_WAIT_HANDSHAKE**: envia hello e espera handshake/config (timeout leva a dormir para economizar).
- **RUNNING_MEASURE**: bursts de ADC (solo, VBAT, NTC) e cálculo de stats brutas.
- **RUNNING_TX / RUNNING_RX**: envia telemetria, tenta receber config/ACK; controla contadores de falha.
- **RUNNING_CONFIG**: aplica/salva config e responde ACK.
- **LOST_RX**: marca perda de RX e decide re-pairing ou dormir.
- **LOW_BATT**: ajusta tempo de sono para poupar energia.
- **DEBUG_LOOP**: ciclo rápido sem deep sleep para depuração.
- **SLEEP**: desliga sensores/LED, programa deep sleep por timer e não retorna.

## Notas importantes
- Telemetria leva apenas valores brutos de ADC (mean/median/min/max/std) por sensor; calibração fica no TGW/GCE.
- O status de bateria (low/med/high) vem do TGW via config (`batt_bucket`); o RSN não classifica VBAT internamente.
- Sem alocação dinâmica; buffers fixos para ESP-NOW e bursts de ADC.
- LED RGB é ânodo comum (LOW acende); divisores de VBAT/NTC têm GND ativo em LOW.
- Deep sleep usa timer; tempos de pwr_up/settling/sampling vêm da config (com defaults).

## Build e upload (PlatformIO)
```sh
pio run -e rsn-node           # compila
pio run -e rsn-node -t upload # compila e grava
pio device monitor            # monitor serial (115200)
```
