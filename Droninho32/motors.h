// ============================================================================
//  Droninho32 — motors.h
//  Controlo dos 4 ESC via PWM (ledc, 50 Hz / 14 bits, como no Teste3).
//  Responsavel por: arm/disarm, acelerador geral, motor individual, mistura
//  quad-X para comandos RC {roll,pitch,yaw,throttle}, rampa e saturacao.
//
//  REGRA DE SEGURANCA: os motores so geram potencia quando armed == true.
//  Desarmado -> todos no pulso minimo (ESC_MIN_US). Ver contrato seccao 7.
// ============================================================================
#ifndef DRONINHO32_MOTORS_H
#define DRONINHO32_MOTORS_H

#include <Arduino.h>
#include "config.h"

namespace motors {

// Inicializa os canais ledc nos pinos dos motores e poe tudo no minimo.
// Deve ser chamada uma vez no setup(), ANTES de qualquer WiFi/servidor.
void begin();

// Arma os motores. So tem efeito se o throttle pedido estiver a 0 (seguranca).
// Devolve true se armou, false se recusou (throttle != 0).
bool arm();

// Desarma: corta a potencia, todos os motores ao pulso minimo imediatamente.
void disarm();

// Paragem de emergencia: equivalente a disarm() mas explicito na intencao.
// Zera alvos e potencia efetiva sem rampa.
void emergencyStop();

// Define o acelerador geral pedido (0..100 %). Saturado a THROTTLE_MAX_TEST.
// Em modo "rc" desliga-se a mistura e usa-se este valor em todos os motores.
void setThrottleAll(int pct);

// Define a potencia de um motor individual (0..3) em 0..100 %. Util para
// testes/calibracao. So atua se armado. Desliga a mistura RC.
void setMotor(uint8_t index, int pct);

// Comando de voo: mistura quad-X. roll/pitch/yaw em -100..100, throttle 0..100.
// Calcula a potencia alvo de cada motor com autoridade MIX_AUTHORITY_PCT.
void setRC(int roll, int pitch, int yaw, int throttle);

// Atualiza a rampa e escreve o PWM. Deve ser chamada periodicamente no loop()
// (a cada MOTOR_UPDATE_MS). Aproxima a potencia efetiva da potencia alvo.
void update();

// --- Leitura de estado (para telemetria) ---
bool    isArmed();
int     throttleGeneral();          // acelerador geral pedido (0..100)
int     motorPercent(uint8_t i);    // potencia EFETIVA do motor i (0..100)

} // namespace motors

#endif // DRONINHO32_MOTORS_H
