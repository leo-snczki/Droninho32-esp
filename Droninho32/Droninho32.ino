// ============================================================================
//  Droninho32 — Firmware unificado do drone (ESP32-S3)
//  Trabalho Final de Curso Profissional de Informatica (PAP).
//
//  Este sketch e MODULAR: cada responsabilidade vive no seu modulo
//  (config / motors / imu / gps / telemetry / comms / web_ui). Aqui apenas
//  orquestramos: inicializamos os modulos e, no loop(), chamamo-los com a
//  cadencia certa. A maquina de estados (idle->armed->flying->failsafe->error)
//  e o failsafe por heartbeat vivem em comms (junto da comunicacao que os
//  dirige) — ver nota de arquitetura em comms.h.
//
//  SEGURANCA (contrato seccao 7 — prioridade maxima):
//   - Arranque DESARMADO, todos os motores no pulso minimo (1000 us).
//   - "arm" so e aceite com o acelerador a 0.
//   - Failsafe: sem comando/heartbeat durante FAILSAFE_MS -> desarma.
//   - {"cmd":"stop"} = paragem de emergencia imediata.
//   - Acelerador limitado a THROTTLE_MAX_TEST + rampa. TESTAR SEM HELICES.
//
//  Compila no Arduino IDE com a placa "ESP32S3 Dev Module" selecionada
//  (FQBN esp32:esp32:esp32s3). Bibliotecas: ver README.md.
// ============================================================================

#include "config.h"
#include "motors.h"
#include "imu.h"
#include "gps.h"
#include "telemetry.h"
#include "comms.h"

// Relogios para tarefas periodicas no loop (millis-based, sem delay() bloqueante).
static uint32_t tMotors = 0;   // atualizacao dos motores / rampa
static uint32_t tImu    = 0;   // leitura da IMU (atitude)
static uint32_t tGps    = 0;   // leitura do GPS

#define IMU_UPDATE_MS  10      // 100 Hz — atitude suave
#define GPS_UPDATE_MS  50      // 20 Hz  — chega para drenar a UART do NEO-6M

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(200);
  Serial.println();
  Serial.print(F("=== ")); Serial.print(FW_NAME);
  Serial.print(F(" v")); Serial.print(FW_VERSION); Serial.println(F(" ==="));

  // 1) MOTORES PRIMEIRO — SEGURANCA: poe os ESC no pulso minimo antes de tudo.
  //    Assim, mesmo que algo abaixo falhe, os motores ficam parados.
  motors::begin();

  // Margem para os ESC armarem com o sinal minimo (como no Teste2/Teste3).
  // Durante este tempo os ESC reconhecem o "throttle a zero".
  delay(2000);

  // 2) Sensores (nao criticos para arrancar — falham para "modo neutro").
  imu::begin();   // MPU6050 (obrigatorio para atitude; se faltar, reporta 0)
  gps::begin();   // NEO-6M (opcional; sem fix reporta valores neutros)

  // 3) Comunicacao por ultimo: SoftAP + WebServer + estado inicial "idle".
  comms::begin();

  Serial.println(F("[BOOT] Pronto. Estado inicial: idle (desarmado)."));
  Serial.print(F("[BOOT] Liga-te ao WiFi \"")); Serial.print(AP_SSID);
  Serial.print(F("\" (pass \"")); Serial.print(AP_PASSWORD);
  Serial.print(F("\") e abre http://")); Serial.println(comms::apIp());
}

void loop() {
  uint32_t now = millis();

  // Servidor HTTP: processa o mais frequentemente possivel.
  comms::handle();

  // IMU @ 100 Hz — mantem a atitude atualizada.
  if (now - tImu >= IMU_UPDATE_MS) {
    tImu = now;
    imu::update();
  }

  // GPS @ 20 Hz — drena a UART e atualiza a posicao.
  if (now - tGps >= GPS_UPDATE_MS) {
    tGps = now;
    gps::update();
  }

  // Motores + failsafe @ 50 Hz — rampa, escrita PWM e maquina de estados.
  if (now - tMotors >= MOTOR_UPDATE_MS) {
    tMotors = now;
    comms::update();    // failsafe por heartbeat + estado logico
    motors::update();   // aplica rampa e escreve o PWM nos ESC
  }
}
