// ============================================================================
//  Droninho32 — config.h
//  Configuracao central do firmware: pinos, PWM, WiFi, sensores, seguranca.
//  Fonte da verdade: docs/ARCHITECTURE.md (seccoes 2, 3, 7, 8).
//
//  Tudo o que precise de ser ajustado para uma placa/montagem diferente deve
//  viver aqui. Os modulos (.cpp) nao definem constantes magicas — leem daqui.
// ============================================================================
#ifndef DRONINHO32_CONFIG_H
#define DRONINHO32_CONFIG_H

#include <Arduino.h>

// ----------------------------------------------------------------------------
//  Identificacao do firmware
// ----------------------------------------------------------------------------
#define FW_NAME       "Droninho32"
#define FW_VERSION    "1.0.0"          // versao do firmware (vai na telemetria/status)

// ----------------------------------------------------------------------------
//  Motores / ESC  (contrato seccao 2 e 8; referencia Teste3)
//  Ordem FIXA dos motores: 0=FL, 1=FR, 2=RL, 3=RR  (frente/tras, esq/dir).
//  Quadcoptero em "X": ver mistura em motors.cpp.
//
//      FL (0)        FR (1)         frente
//          \\        //
//           \\      //
//            ESP32-S3
//           //      \\
//          //        \\
//      RL (2)        RR (3)         tras
// ----------------------------------------------------------------------------
#define MOTOR_COUNT   4

// Pinos PWM dos ESC, na ordem FL, FR, RL, RR (iguais ao Teste3: 10,11,12,13).
static const uint8_t MOTOR_PINS[MOTOR_COUNT] = { 10, 11, 12, 13 };

// Indices simbolicos (para legibilidade na mistura quad-X)
#define M_FL 0
#define M_FR 1
#define M_RL 2
#define M_RR 3

// Parametros do sinal PWM dos ESC (igual ao Teste3: ledc 50 Hz, 14 bits).
// Os ESC comuns esperam um pulso de 1000..2000 us a 50 Hz (periodo de 20 ms).
#define PWM_FREQ_HZ      50            // 50 Hz -> periodo de 20000 us
#define PWM_RES_BITS     14            // 14 bits -> duty maximo = 2^14 - 1 = 16383
#define PWM_PERIOD_US    20000         // periodo em microssegundos (1/50 Hz)
#define PWM_MAX_DUTY     16383         // (1 << PWM_RES_BITS) - 1

// Pulsos dos ESC em microssegundos.
#define ESC_MIN_US       1000          // motor parado / armado (pulso minimo)
#define ESC_MAX_US       2000          // potencia maxima teorica

// ----------------------------------------------------------------------------
//  Limites de SEGURANCA (contrato seccao 7 — PRIORIDADE MAXIMA)
// ----------------------------------------------------------------------------
// Limite de acelerador em testes: 0..100 (%). Comeca BAIXO. So subir depois de
// validar SEM helices. Saturamos qualquer pedido a este valor.
#define THROTTLE_MAX_TEST   100        // % — SEM limite de potencia (a pedido). ATENCAO: testa SEMPRE sem helices!

// Rampa: variacao maxima de potencia (em %) por ciclo de atualizacao dos motores.
// Evita saltos bruscos de RPM. Com MOTOR_UPDATE_MS=20ms e step=2%, sobe ~100%/s.
#define THROTTLE_RAMP_STEP  2          // % por ciclo de motors.update()

// Periodo de atualizacao da rampa dos motores (ms). loop() chama com este passo.
#define MOTOR_UPDATE_MS     20         // 50 Hz de atualizacao logica (coincide com PWM)

// Failsafe: se nao chegar nenhum comando/heartbeat durante este tempo, desarma.
// A app envia "heartbeat" a ~2 Hz, logo 1500 ms cobre varios pacotes perdidos.
#define FAILSAFE_MS         1500       // ms sem comando -> failsafe

// Autoridade de mistura RC em % (quanto roll/pitch/yaw podem desviar cada motor
// em torno do throttle base). Conservador para testes.
#define MIX_AUTHORITY_PCT   20         // +-20% maximo de correcao por eixo somado

// ----------------------------------------------------------------------------
//  WiFi SoftAP  (contrato seccao 8)
// ----------------------------------------------------------------------------
#define AP_SSID       "Droninho32"     // SSID do Access Point do drone
#define AP_PASSWORD   "droninho32"     // password (>= 8 chars, exigido pelo ESP32)
#define AP_CHANNEL    6                // canal WiFi
#define AP_MAX_CONN   2                // ligacoes simultaneas (app + debug)
#define HTTP_PORT     80               // porta do WebServer

// IP fixo do AP (contrato: 192.168.4.1). E o default do ESP32 em SoftAP,
// mas fixamo-lo explicitamente para nao depender de defaults.
#define AP_IP_0  192
#define AP_IP_1  168
#define AP_IP_2  4
#define AP_IP_3  1

// ----------------------------------------------------------------------------
//  ESP32-CAM (placa SEPARADA — liga-se a este AP como estacao em IP fixo).
//  O S3 nao comunica com a camara; apenas ANUNCIA o URL em /api/status para a app.
// ----------------------------------------------------------------------------
#define CAMERA_IP            "192.168.4.2"
#define CAMERA_URL           "http://192.168.4.2/stream"   // video MJPEG
#define CAMERA_SNAPSHOT_URL  "http://192.168.4.2/jpg"      // imagem unica

// ----------------------------------------------------------------------------
//  IMU — MPU6050 / GY-521 via I2C
//  No ESP32-S3 os pinos I2C sao remapeaveis. Usamos os defaults do core
//  (Wire) para o S3: SDA=GPIO8, SCL=GPIO9. Ajusta aqui se ligaste noutros.
//  (Documentado no README — esquema de ligacoes.)
// ----------------------------------------------------------------------------
#define IMU_I2C_SDA      8             // GPIO8  (default Wire no ESP32-S3)
#define IMU_I2C_SCL      9             // GPIO9  (default Wire no ESP32-S3)
#define IMU_I2C_ADDR     0x68          // endereco I2C do MPU6050 (AD0=GND)
#define IMU_I2C_FREQ     400000        // 400 kHz (fast mode)

// Filtro complementar: peso do giroscopio na fusao (0..1). 0.98 = confia mais
// no giroscopio a curto prazo e usa o acelerometro para corrigir a longo prazo.
#define IMU_COMP_ALPHA   0.98f

// ----------------------------------------------------------------------------
//  GPS — NEO-6M via UART (Serial1)
//  RX/TX referem-se ao lado do ESP32. Liga o TX do NEO-6M ao RX do ESP32.
//  Pinos escolhidos livres no S3 (documentado no README).
// ----------------------------------------------------------------------------
#define GPS_UART_NUM     1             // usa Serial1 (UART1)
#define GPS_RX_PIN       18            // ESP32 RX  <- TX do NEO-6M
#define GPS_TX_PIN       17            // ESP32 TX  -> RX do NEO-6M (normalmente nao usado)
#define GPS_BAUD         9600          // baud rate por defeito do NEO-6M

// ----------------------------------------------------------------------------
//  Bateria (opcional — divisor resistivo num ADC). Desligado por defeito:
//  sem hardware de medicao, a telemetria reporta valores neutros (0).
//  Para ativar: BATTERY_ENABLED 1 e liga um divisor de tensao ao BATTERY_PIN.
// ----------------------------------------------------------------------------
#define BATTERY_ENABLED  0             // 0 = sem medicao (reporta 0V / 0%)
#define BATTERY_PIN      4             // ADC1 (ajustar se ativado)
#define BATTERY_DIV      4.0f          // fator do divisor p/ 3S: Vbat = Vadc * 4 (12.6 V -> ~3.15 V no ADC). Dimensiona as resistencias para ficar < 3.3 V.
#define BATTERY_VMIN     9.0f          // 3S vazia (~3.0 V/celula)
#define BATTERY_VMAX     11.7f         // 3S cheia (medido ~11.7 V neste pack)

// ----------------------------------------------------------------------------
//  Serial de depuracao
// ----------------------------------------------------------------------------
#define SERIAL_BAUD      115200        // monitor serie (igual a todos os Testes)

// ----------------------------------------------------------------------------
//  Funcoes utilitarias de conversao (inline — partilhadas pelos modulos)
// ----------------------------------------------------------------------------

// Converte percentagem de acelerador (0..100) para pulso de ESC (1000..2000 us).
inline uint16_t throttlePctToUs(int pct) {
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  return (uint16_t)(ESC_MIN_US + ((uint32_t)(ESC_MAX_US - ESC_MIN_US) * pct) / 100);
}

// Converte um pulso em microssegundos para o valor de duty do ledc (14 bits).
// Igual a formula do Teste3: duty = us * (2^14-1) / periodo.
inline uint32_t usToDuty(uint16_t us) {
  return ((uint32_t)us * PWM_MAX_DUTY) / PWM_PERIOD_US;
}

// Converte diretamente percentagem (0..100) para duty do ledc.
inline uint32_t throttlePctToDuty(int pct) {
  return usToDuty(throttlePctToUs(pct));
}

#endif // DRONINHO32_CONFIG_H
