# RSN ↔ TGW ↔ GCE – Binary Protocol

Versão: 6.0  
Escopo: descreve o **contrato binário estável** entre:

- **RSN** – Remote Sensor Node (ESP32, sensores analógicos)
- **TGW** – Telemetry GateWay (ESP32, ESP-NOW + Serial)
- **GCE** – Graphical Control & Evaluation (Python)

Este documento **deve bater 1:1** com:

- `constants.h` / `tgw_constants.h`
- `proto_helpers.cpp` (RSN)
- `tgw_proto.cpp`, `tgw_logic.cpp`
- `rsn_proto.py` (lado GCE)

Endianness: **little-endian** para todos os campos multi-byte.  
Alinhamento: todas as `struct` C marcadas com `__attribute__((packed))`.

---

## 1. RSN ↔ TGW (ESP-NOW)

### 1.1. Tipos de pacote (`rsn_packet_type_t`)

| Tipo       | Valor | Direção      | Descrição                                  |
|-----------|-------|--------------|--------------------------------------------|
| HELLO     | 0x01  | RSN → TGW    | Anúncio de RSN em modo pairing             |
| HANDSHAKE | 0x02  | TGW → RSN    | Atribui `node_id` ao RSN                   |
| TELEMETRY | 0x03  | RSN → TGW    | Telemetria bruta (ADC)                     |
| CONFIG    | 0x04  | TGW → RSN    | Configuração operacional do nó             |
| CONFIG_ACK| 0x05  | RSN → TGW    | Confirmação de aplicação de config         |
| DEBUG     | 0x06  | RSN → TGW    | Pacote opcional de debug                   |

> **Tamanho máximo** de qualquer pacote EPS-NOW: `RSN_MAX_PACKET_SIZE = 128` bytes.

---

### 1.2. Cabeçalho comum: `rsn_header_t` (5 bytes)

Usado em **todos** os pacotes.

```c
typedef struct __attribute__((packed)) {
    uint8_t  type;       // rsn_packet_type_t
    uint8_t  node_id;    // 0 se ainda não configurado
    uint8_t  mode;       // rsn_mode_t (RUNNING / PAIRING / DEBUG)
    uint8_t  hw_version; // RSN_HW_VERSION
    uint8_t  fw_version; // RSN_FW_VERSION
} rsn_header_t;
Tamanho: 5 bytes

1.3. HELLO (RSN → TGW)
c
Copiar código
typedef struct __attribute__((packed)) {
    rsn_header_t hdr;     // type = RSN_PKT_HELLO, node_id = 0 em pairing
    uint16_t     capabilities; // RSN_CAP_SOIL | RSN_CAP_VBAT | RSN_CAP_NTC | RSN_CAP_RGB...
} rsn_hello_packet_t;
Tamanho: 7 bytes.

Enviado por RSN em modo PAIRING, com MAC = FF:FF:FF:FF:FF:FF (broadcast).

hdr.node_id deve ser RSN_NODE_ID_UNSET (0) durante pairing.

TGW, ao receber:

Lê hdr.node_id (pode ser 0 no início).

Atualiza tabela interna de nós com MAC e último RSSI.

Repassa HELLO para o GCE via frame UP_RSN_HELLO (ver seção 2).

1.4. HANDSHAKE (TGW → RSN)
c
Copiar código
typedef struct __attribute__((packed)) {
    rsn_header_t hdr;  // hdr.type = RSN_PKT_HANDSHAKE, hdr.node_id = ID atribuído
} rsn_handshake_packet_t;
Tamanho: 5 bytes.

hdr.node_id: novo ID do nó escolhido no GCE (e repassado pelo TGW).

hdr.mode: normalmente RSN_MODE_RUNNING após pairing.

RSN, ao receber:

Pode usar o node_id do HANDSHAKE para identificar-se.

Em v6.x, o RSN também aceita o node_id vindo diretamente no header do CONFIG se o HANDSHAKE se perder – ver abaixo.

1.5. CONFIG (TGW → RSN)
c
Copiar código
typedef struct __attribute__((packed)) {
    rsn_header_t hdr;            // type = RSN_PKT_CONFIG
    uint16_t sleep_time_s;       // tempo de sono entre ciclos
    uint16_t pwr_up_time_ms;     // delay para energizar sensores
    uint16_t settling_time_ms;   // delay entre ligar sensor e iniciar leitura
    uint16_t sampling_interval_ms; // intervalo entre amostras dentro do burst
    uint8_t  led_mode_default;   // reservado para políticas de LED
    uint8_t  batt_bucket;        // reservado para política de bateria
    uint8_t  lostRx_limit;       // nº de falhas RX antes de entrar em LOST_RX
    uint8_t  debug_mode;         // 0 = normal, 1 = debug_loop
    uint8_t  reset_flags;        // bitmask para reset de contadores/flags
} rsn_config_packet_t;
Tamanho: 18 bytes.

hdr.node_id deve conter o ID final do RSN escolhido pelo GCE.

RSN, ao receber:

Copia a struct inteira para g_rsn_config.

Marca config_valid = true.

Se estava em waiting_handshake e o HANDSHAKE não chegou, aceita o node_id do header do CONFIG, limpa waiting_handshake e passa a usar esse ID em CONFIG_ACK e TELEMETRY.

1.6. CONFIG_ACK (RSN → TGW)
c
Copiar código
typedef struct __attribute__((packed)) {
    rsn_header_t hdr;  // type = RSN_PKT_CONFIG_ACK
    uint8_t      status;  // 0 = OK, !=0 = erro ao aplicar config
} rsn_config_ack_packet_t;
Tamanho: 6 bytes.

RSN envia após aplicar a CONFIG (salvar em NVS, atualizar flags).

TGW, ao receber:

Atualiza estado interno do nó.

Repassa o ACK bruto para o GCE via frame UP_RSN_CONFIG_ACK.

1.7. TELEMETRY (RSN → TGW)
Somente valores brutos (ADC) – calibração é responsabilidade do GCE.

c
Copiar código
typedef struct __attribute__((packed)) {
    rsn_header_t hdr;        // type = RSN_PKT_TELEMETRY

    uint32_t cycle;          // contador de ciclos do RSN
    uint32_t ts_ms;          // timestamp local (millis() do RSN)

    uint8_t  batt_status;    // rsn_batt_status_t (LOW/MED/HIGH)
    uint8_t  flags;          // bitmask rsn_telem_flags_t

    // Solo (ADC bruto)
    uint16_t soil_mean_raw;
    uint16_t soil_median_raw;
    uint16_t soil_min_raw;
    uint16_t soil_max_raw;
    uint16_t soil_std_raw;

    // VBAT (ADC bruto)
    uint16_t vbat_mean_raw;
    uint16_t vbat_median_raw;
    uint16_t vbat_min_raw;
    uint16_t vbat_max_raw;
    uint16_t vbat_std_raw;

    // NTC (ADC bruto)
    uint16_t ntc_mean_raw;
    uint16_t ntc_median_raw;
    uint16_t ntc_min_raw;
    uint16_t ntc_max_raw;
    uint16_t ntc_std_raw;

    int8_t   last_rssi;      // 0x7F se indisponível (RSN → TGW)
} rsn_telemetry_packet_t;
Tamanho: 46 bytes.

RSN preenche stats brutos para solo, VBAT e NTC (sem calibrar).

flags combina RSN_TF_* (low_batt, lost_rx, debug_mode, watchdog, brownout, etc.).

TGW, ao receber:

Descarta qualquer TELEMETRY cujo comprimento len != sizeof(rsn_telemetry_packet_t) para evitar lixo/truncamento.

Se uplink GCE estiver “conectado”, empacota em UP_RSN_TELEMETRY.

Caso contrário, enfileira em tgw_store para envio posterior.

1.8. DEBUG (RSN → TGW) – opcional
c
Copiar código
typedef struct __attribute__((packed)) {
    rsn_header_t hdr;
    uint16_t     rx_failed;
    uint8_t      num_soil_raw;
    uint16_t     soil_raw[16];
} rsn_debug_packet_t;
Não é crítico para operação básica.

Pode ser ignorado pelo TGW ou repassado futuramente ao GCE.

2. TGW ↔ GCE (Serial / Uplink)
Transport layer:

Framing TGW → GCE e GCE → TGW é sempre:

[len_LSB][len_MSB][payload...]

len = tamanho do payload (uint16 little-endian).

Sem checksum por enquanto; campo len + tipos bem definidos reduzem risco de dessincronização.

2.1. Códigos de frame (payload[0])
Uplink (TGW → GCE)

Nome	Valor	Descrição
UP_RSN_HELLO	0xA1	Encapsula rsn_hello_packet_t
UP_RSN_TELEMETRY	0xA2	Encapsula rsn_telemetry_packet_t
UP_RSN_CONFIG_ACK	0xA3	Encapsula rsn_config_ack_packet_t

Downlink (GCE → TGW)

Nome	Valor	Descrição
DOWN_RSN_CONFIG	0xB1	Envia rsn_config_packet_t para um RSN
DOWN_RSN_HANDSHAKE	0xB2	Opcional: força HANDSHAKE/ID em um RSN

2.2. Uplink: UP_RSN_HELLO (0xA1)
Payload:

text
Copiar código
[0]  frame_type = 0xA1
[1]  node_id    (uint8)
[2]  rssi       (int8, cast simples do WiFi.RSSI)
[3..] raw_hello (rsn_hello_packet_t, 7 bytes)
Tamanho do payload: 1 + 1 + 1 + 7 = 10 bytes
Tamanho total no fio: 2 (len) + 10 (payload) = 12 bytes.

Semântica:

GCE recebe o HELLO original mais metadata de transporte:

node_id (pode ser 0 em pairing).

rssi medido no TGW.

2.3. Uplink: UP_RSN_TELEMETRY (0xA2)
Payload:

text
Copiar código
[0]   frame_type = 0xA2
[1]   node_id    (uint8)
[2]   rssi       (int8, RSSI no TGW)
[3..6] tgw_local_ts_ms (uint32 little-endian, millis() no TGW)
[7..] raw_telem (rsn_telemetry_packet_t, 46 bytes)
Tamanho do payload: 1 + 1 + 1 + 4 + 46 = 53 bytes
Tamanho total no fio: 2 + 53 = 55 bytes.

Notas:

tgw_local_ts_ms é um campo extra que não existe no RSN.

Serve para o GCE reconstruir a linha do tempo mesmo com RSN dormindo/deep-sleep.

raw_telem é o pacote C rsn_telemetry_packet_t sem alterações.

2.4. Uplink: UP_RSN_CONFIG_ACK (0xA3)
Payload:

text
Copiar código
[0]  frame_type = 0xA3
[1]  node_id    (uint8)
[2]  rssi       (int8)
[3..] raw_ack   (rsn_config_ack_packet_t, 6 bytes)
Tamanho do payload: 1 + 1 + 1 + 6 = 9 bytes
Tamanho total no fio: 2 + 9 = 11 bytes.

2.5. Downlink: DOWN_RSN_CONFIG (0xB1)
Payload GCE → TGW:

text
Copiar código
[0]  frame_type = 0xB1
[1]  node_id    (uint8)
[2..] cfg       (rsn_config_packet_t, 18 bytes)
Tamanho do payload: 1 + 1 + 18 = 20 bytes
Tamanho total no fio: 2 + 20 = 22 bytes.

Fluxo:

GCE monta rsn_config_packet_t com:

hdr.type = RSN_PKT_CONFIG

hdr.node_id = node_id

hdr.mode tipicamente RSN_MODE_RUNNING

hw_version / fw_version compatíveis.

GCE envia frame 0xB1.

TGW:

extrai node_id e cfg.

salva cfg em NVS (tgw_store_save_node_config).

envia o cfg via ESP-NOW para o MAC do nó (tgw_proto_send_to_node).

RSN, ao receber o CONFIG:

Aplica a config, atualiza NVS, responde com CONFIG_ACK.

2.6. Downlink: DOWN_RSN_HANDSHAKE (0xB2) – opcional
Duplo modo, conforme o que o GCE quiser:

Modo A – payload curto
text
Copiar código
[0] frame_type = 0xB2
[1] node_id    (uint8)
TGW recebe, constrói um rsn_handshake_packet_t novo:

hdr.type = RSN_PKT_HANDSHAKE

hdr.node_id = node_id

hdr.mode = RSN_MODE_RUNNING

hw_version / fw_version do lado TGW/RSN esperado.

TGW envia esse HANDSHAKE via ESP-NOW para MAC conhecido do nó.

Modo B – payload completo
text
Copiar código
[0] frame_type = 0xB2
[1] node_id    (uint8)
[2..] raw_hs   (rsn_handshake_packet_t, 5 bytes)
GCE monta o rsn_handshake_packet_t exatamente como deseja.

TGW apenas repassa o raw_hs para o RSN.

Em ambos os modos, node_id em [1] deve bater com hdr.node_id do handshake.

3. Responsabilidades por camada
3.1. RSN
Medir sensores, calcular stats brutas (ADC).

Aplicar CONFIG recebida e responder CONFIG_ACK.

Manter máquina de estados (BOOT, CHECK_CONFIG, PAIRING, RUNNING, DEBUG, SLEEP).

Pairing:

Enviar HELLO em broadcast.

Aceitar node_id vindo:

do HANDSHAKE (ideal), ou

do header de CONFIG (fallback se handshake perdido).

3.2. TGW
ESP-NOW front-end:

Receber HELLO/TELEMETRY/CONFIG_ACK/DEBUG.

Enviar CONFIG/HANDSHAKE para MAC de cada nó.

Uplink:

Serial framing [len][len][payload].

Encapsular pacotes RSN em frames UP_... para o GCE.

Receber frames DOWN_... e traduzi-los em CONFIG/HANDSHAKE para RSN.

Buffer offline:

Se tgw_uplink_is_connected() for false, empilhar telemetrias em FIFO.

Ao reconectar, escoar FIFO e enviar ao GCE.

3.3. GCE
Única fonte da verdade de:

node_id de cada RSN.

Configurações por nó (rsn_config_packet_t).

Interpretar:

HELLO (descobertas/pairing).

TELEMETRY bruta → calibração, plots, alertas.

CONFIG_ACK (verificar sucesso de aplicação).

Gerar CONFIG e, opcionalmente, HANDSHAKE.

4. Invariantes e checks importantes
Tamanhos fixos

sizeof(rsn_header_t) == 5

sizeof(rsn_hello_packet_t) == 7

sizeof(rsn_config_packet_t) == 18

sizeof(rsn_config_ack_packet_t) == 6

sizeof(rsn_telemetry_packet_t) == 46

GCE (rsn_proto.py) deve usar formatos struct compatíveis (little-endian, sem padding).

Len check na TELEMETRY

TGW só deve aceitar TELEMETRY com len == sizeof(rsn_telemetry_packet_t).

Qualquer payload menor/maior deve ser descartado e não repassado ao GCE.

HELLO em broadcast

RSN envia HELLO sempre com MAC broadcast (FF:FF:FF:FF:FF:FF).

Evita ficar “preso” a um TGW antigo/desconhecido.

Aceitar node_id via CONFIG

Se waiting_handshake == true e chega CONFIG com hdr.node_id != 0, RSN:

assume esse node_id,

limpa waiting_handshake,

passa a usar esse ID em CONFIG_ACK/TELEMETRY.

Compatibilidade de versões

GCE deve conferir hw_version / fw_version no header antes de aplicar configs agressivas.

Se detectar mismatch crítico, pode rejeitar ou marcar nó como “incompatível”.