#pragma once                 // Evita inclusao dupla do header.

#include <stdint.h>          // Tipos inteiros de largura fixa.
#include <stdbool.h>         // Tipo booleano padrao C.

// ============================================================================
//  Mapeamento de GPIOs
// ============================================================================

#define PIN_NTC_SENSE             36  // ADC1_CH0 para ler divisor NTC (temperatura).
#define PIN_SOIL_ADC              39  // ADC1_CH3 para leitura do sensor capacitivo de solo.
#define PIN_VBAT_SENSE            33  // ADC1_CH5 para ler tensao do divisor de bateria.

#define PIN_SENSE_EN              32  // Saida que liga o MOSFET high-side do sensor de solo (HIGH = ligado).
#define PIN_VBAT_GND_EN           25  // Saida que fecha GND do divisor de VBAT (LOW = divisor ativo).
#define PIN_NTC_GND_EN            26  // Saida que fecha GND do divisor do NTC (LOW = divisor ativo).

#define PIN_LED_RED               17  // Saida para LED vermelho (anodo comum, LOW acende).
#define PIN_LED_GREEN             16  // Saida para LED verde (anodo comum, LOW acende).
#define PIN_LED_BLUE               4  // Saida para LED azul (anodo comum, LOW acende).

// ============================================================================
//  Macros de conveniencia (sensores / divisores / LEDs)
// ============================================================================

#define VBAT_GND_ON()    digitalWrite(PIN_VBAT_GND_EN, LOW)  // Ativa divisor de VBAT fechando GND.
#define VBAT_GND_OFF()   digitalWrite(PIN_VBAT_GND_EN, HIGH) // Desativa divisor de VBAT abrindo GND.

#define NTC_GND_ON()     digitalWrite(PIN_NTC_GND_EN, LOW)   // Ativa divisor do NTC fechando GND.
#define NTC_GND_OFF()    digitalWrite(PIN_NTC_GND_EN, HIGH)  // Desativa divisor do NTC abrindo GND.

#define SOIL_ON()        digitalWrite(PIN_SENSE_EN, HIGH)    // Liga sensor de solo energizando high-side.
#define SOIL_OFF()       digitalWrite(PIN_SENSE_EN, LOW)     // Desliga sensor de solo cortando energia.

#define LED_RED_ON()     digitalWrite(PIN_LED_RED, LOW)      // Acende LED vermelho (LOW em anodo comum).
#define LED_RED_OFF()    digitalWrite(PIN_LED_RED, HIGH)     // Apaga LED vermelho (HIGH em anodo comum).
#define LED_GREEN_ON()   digitalWrite(PIN_LED_GREEN, LOW)    // Acende LED verde.
#define LED_GREEN_OFF()  digitalWrite(PIN_LED_GREEN, HIGH)   // Apaga LED verde.
#define LED_BLUE_ON()    digitalWrite(PIN_LED_BLUE, LOW)     // Acende LED azul.
#define LED_BLUE_OFF()   digitalWrite(PIN_LED_BLUE, HIGH)    // Apaga LED azul.

// ============================================================================
//  Constantes gerais de firmware
// ============================================================================

#define RSN_HW_VERSION        1u   // Versao de hardware conhecida pelo firmware.
#define RSN_FW_VERSION        1u   // Versao de firmware reportada.
#define RSN_MAX_PACKET_SIZE   128u // Tamanho maximo suportado para buffers de pacote.
#define RSN_NODE_ID_UNSET     0u   // Node ID nao configurado (estado inicial).

#define RSN_DEFAULT_SLEEP_S       3u   // Tempo de sono padrao em segundos se nao houver config (acelera pairing).
#define RSN_DEFAULT_PWR_UP_MS     100u // Tempo de power-up minimo para sensores em ms.
#define RSN_DEFAULT_SETTLE_MS     150u // Tempo de settling dos divisores em ms.
#define RSN_DEFAULT_SAMPLE_MS     50u  // Intervalo entre amostragens consecutivas dentro do ciclo em ms.
#define RSN_DEFAULT_LOSTRX_LIMIT  3u   // Limite default de falhas RX antes de marcar lost RX.
#define RSN_DEFAULT_NUM_SAMPLES   4u   // Numero padrao de amostras por burst ADC.
#define RSN_MAX_ADC_SAMPLES       16u  // Limite maximo suportado de amostras por burst (buffer estatico).
#define RSN_DEFAULT_LOG_DEBUG     1u   // Habilita logs seriais detalhados por padrao.

#define RSN_CAP_SOIL (1u << 0) // Bitmask de capacidades: suporte a solo.
#define RSN_CAP_VBAT (1u << 1) // Bitmask de capacidades: suporte a leitura de bateria.
#define RSN_CAP_NTC  (1u << 2) // Bitmask de capacidades: suporte a NTC.
#define RSN_CAP_RGB  (1u << 3) // Bitmask de capacidades: suporte a LED RGB.

// ============================================================================
//  Modos de operacao e estados da maquina
// ============================================================================

typedef enum : uint8_t {
    RSN_MODE_RUNNING = 0,   // Execucao normal com medicao e envio.
    RSN_MODE_PAIRING = 1,   // Fase de pareamento aguardando TGW.
    RSN_MODE_DEBUG   = 2    // Modo debug com ciclo rapido e sem deep sleep.
} rsn_mode_t;               // Modo logico do no.

typedef enum : uint8_t {
    RSN_STATE_BOOT = 0,                 // Inicio de boot.
    RSN_STATE_CHECK_CONFIG,             // Verifica se existe config valida.
    RSN_STATE_RUNNING_MEASURE,          // Coleta de sensores.
    RSN_STATE_RUNNING_TX,               // Envio de telemetria.
    RSN_STATE_RUNNING_RX,               // Espera respostas/config.
    RSN_STATE_RUNNING_CONFIG,           // Aplica nova config.
    RSN_STATE_PAIRING_HELLO,            // Envia hello para TGW.
    RSN_STATE_PAIRING_WAIT_HANDSHAKE,   // Aguarda handshake de retorno.
    RSN_STATE_LOST_RX,                  // Modo de erro por RX perdido.
    RSN_STATE_LOW_BATT,                 // Estado para bateria baixa.
    RSN_STATE_DEBUG_LOOP,               // Loop especial de debug sem sleep.
    RSN_STATE_SLEEP                     // Prepara e entra em deep sleep.
} rsn_state_t;                          // Estados da maquina principal.

// ============================================================================
//  Status de bateria e flags de telemetria
// ============================================================================

typedef enum : uint8_t {
    RSN_BATT_LOW  = 0,  // Bateria baixa.
    RSN_BATT_MED  = 1,  // Bateria media.
    RSN_BATT_HIGH = 2   // Bateria alta.
} rsn_batt_status_t;    // Faixas de bateria usadas nos pacotes e LED.

typedef enum : uint8_t {
    RSN_TF_LOW_BATT      = (1u << 0), // Indicador de bateria baixa.
    RSN_TF_LOST_RX       = (1u << 1), // Indicador de perda de RX/ACK.
    RSN_TF_DEBUG_MODE    = (1u << 2), // No em modo debug.
    RSN_TF_WATCHDOG_RST  = (1u << 3), // Ultimo reset foi watchdog.
    RSN_TF_BROWNOUT_RST  = (1u << 4), // Ultimo reset foi brownout.
    RSN_TF_FIRST_BOOT    = (1u << 5)  // Flag para primeiro boot/boot limpo.
    // bits 6 e 7 reservados para futuro.
} rsn_telem_flags_t;                 // Flags enviados na telemetria.

// ============================================================================
//  Estatistica basica de ADC (uso interno e telemetria)
// ============================================================================

typedef struct {
    uint16_t mean;    // Media das leituras brutas.
    uint16_t median;  // Mediana das leituras brutas.
    uint16_t min;     // Menor valor lido.
    uint16_t max;     // Maior valor lido.
    uint16_t stddev;  // Desvio padrao das leituras.
    uint8_t  count;   // Numero de amostras consideradas.
} rsn_adc_stats_t;    // Estrutura de estatisticas de burst ADC.

// ============================================================================
//  Tipos de pacote de comunicacao (ESP-NOW)
// ============================================================================

typedef enum : uint8_t {
    RSN_PKT_HELLO      = 0x01, // Hello emitido pelo RSN.
    RSN_PKT_HANDSHAKE  = 0x02, // Handshake vindo do TGW.
    RSN_PKT_TELEMETRY  = 0x03, // Telemetria do RSN.
    RSN_PKT_CONFIG     = 0x04, // Config enviada pelo TGW.
    RSN_PKT_CONFIG_ACK = 0x05, // ACK de config devolvido pelo RSN.
    RSN_PKT_DEBUG      = 0x06  // Pacote opcional de debug.
} rsn_packet_type_t;           // Identificadores de pacotes ESP-NOW.

// ============================================================================
//  Cabecalho comum de todos os pacotes
// ============================================================================

typedef struct __attribute__((packed)) {
    uint8_t  type;       // Tipo do pacote (rsn_packet_type_t).
    uint8_t  node_id;    // ID do no ou 0 se ainda nao configurado.
    uint8_t  mode;       // Modo atual (rsn_mode_t).
    uint8_t  hw_version; // Versao de hardware reportada.
    uint8_t  fw_version; // Versao de firmware reportada.
} rsn_header_t;          // Cabecalho padrao de todos os pacotes.

// ============================================================================
//  Estruturas dos pacotes
// ============================================================================

typedef struct __attribute__((packed)) {
    rsn_header_t hdr;     // Cabecalho padrao.
    uint16_t     capabilities;   // Bitmask de recursos suportados.
} rsn_hello_packet_t;           // Pacote Hello enviado do RSN para o TGW.

typedef struct __attribute__((packed)) {
    rsn_header_t hdr;     // Cabecalho padrao com node_id definido pelo TGW.
} rsn_handshake_packet_t;       // Handshake/ack de pairing vindo do TGW.

typedef struct __attribute__((packed)) {
    rsn_header_t hdr;            // Cabecalho padrao.
    uint16_t sleep_time_s;       // Tempo de sono programado (s).
    uint16_t pwr_up_time_ms;     // Tempo para energizar sensores (ms).
    uint16_t settling_time_ms;   // Tempo para estabilizar divisores (ms).
    uint16_t sampling_interval_ms; // Intervalo entre amostras internas (ms).
    uint8_t  led_mode_default;   // Config de LED padrao.
    uint8_t  batt_bucket;        // Faixa de bateria vinda do TGW.
    uint8_t  lostRx_limit;       // Limite de falhas RX antes de erro.
    uint8_t  debug_mode;         // Habilita ou nao modo debug.
    uint8_t  reset_flags;        // Bitmask pedindo reset de contadores/flags.
} rsn_config_packet_t;          // Pacote de configuracao do TGW para o RSN.

typedef struct __attribute__((packed)) {
    rsn_header_t hdr;     // Cabecalho padrao.
    uint8_t      status;  // 0 = OK, diferente de zero = erro ao aplicar config.
} rsn_config_ack_packet_t; // Pacote de ACK de configuracao devolvido.

typedef struct __attribute__((packed)) {
    rsn_header_t hdr;            // Cabecalho padrao.
    uint32_t cycle;              // Contador de ciclos executados.
    uint32_t ts_ms;              // Timestamp relativo em ms.
    uint8_t  batt_status;        // Status da bateria (rsn_batt_status_t).
    uint8_t  flags;              // Flags de telemetria (rsn_telem_flags_t).
    uint16_t soil_mean_raw;      // Media bruta do ADC do sensor de solo.
    uint16_t soil_median_raw;    // Mediana bruta do ADC do sensor de solo.
    uint16_t soil_min_raw;       // Minimo bruto do ADC do sensor de solo.
    uint16_t soil_max_raw;       // Maximo bruto do ADC do sensor de solo.
    uint16_t soil_std_raw;       // Desvio padrao bruto do ADC do sensor de solo.
    uint16_t vbat_mean_raw;      // Media bruta do ADC de VBAT.
    uint16_t vbat_median_raw;    // Mediana bruta do ADC de VBAT.
    uint16_t vbat_min_raw;       // Minimo bruto do ADC de VBAT.
    uint16_t vbat_max_raw;       // Maximo bruto do ADC de VBAT.
    uint16_t vbat_std_raw;       // Desvio padrao bruto do ADC de VBAT.
    uint16_t ntc_mean_raw;       // Media bruta do ADC do NTC.
    uint16_t ntc_median_raw;     // Mediana bruta do ADC do NTC.
    uint16_t ntc_min_raw;        // Minimo bruto do ADC do NTC.
    uint16_t ntc_max_raw;        // Maximo bruto do ADC do NTC.
    uint16_t ntc_std_raw;        // Desvio padrao bruto do ADC do NTC.
    int8_t   last_rssi;          // Ultimo RSSI observado ou 0x7F se indisponivel.
} rsn_telemetry_packet_t;        // Pacote de telemetria do RSN contendo apenas valores brutos.

typedef struct __attribute__((packed)) {
    rsn_header_t hdr;       // Cabecalho padrao.
    uint16_t     rx_failed; // Contador de falhas de RX.
    uint8_t      num_soil_raw; // Quantidade de amostras brutas de solo.
    uint16_t     soil_raw[16]; // Amostras brutas de solo (modo debug).
} rsn_debug_packet_t;        // Pacote opcional de debug.

// ============================================================================
//  Estado interno do RSN (runtime/persistencia leve)
// ============================================================================

typedef struct {
    uint8_t  node_id;           // ID do no atribuido pelo TGW.
    bool     config_valid;      // Indica se ha config valida salva.
    bool     debug_mode;        // Marca se esta em modo debug.
    bool     low_batt_flag;     // Flag de bateria baixa.
    bool     lost_rx_flag;      // Flag de perda de RX recente.
    bool     waiting_handshake; // Ainda aguardando handshake de pareamento.
    bool     waiting_config;    // Aguardando config apos handshake.
    uint8_t  last_reset_cause;  // Ultima causa de reset (esp_reset_reason).
    uint16_t rx_failed;         // Falhas acumuladas de RX/ACK.
    uint32_t cycle_count;       // Ciclos de operacao realizados.
} rsn_runtime_status_t;        // Estrutura compacta com status do no.
