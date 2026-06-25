// ============================================================================
//  Droninho32 — imu.h
//  Leitura do MPU6050 (GY-521) via I2C usando a biblioteca Adafruit MPU6050
//  (depende de Adafruit Unified Sensor). Calcula atitude (roll/pitch/yaw)
//  com um filtro complementar.
//
//  MVP: a IMU e o sensor obrigatorio. Se nao estiver ligada, begin() devolve
//  false e a telemetria reporta valores neutros (0) — o resto do firmware
//  continua a funcionar (contrato seccao 9).
// ============================================================================
#ifndef DRONINHO32_IMU_H
#define DRONINHO32_IMU_H

#include <Arduino.h>
#include "config.h"

namespace imu {

// Leitura crua + atitude estimada. Unidades conforme contrato (seccao 2):
//   ax,ay,az em g ; gx,gy,gz em graus/s ; roll,pitch,yaw em graus.
struct Reading {
  float ax = 0, ay = 0, az = 0;     // aceleracao (g)
  float gx = 0, gy = 0, gz = 0;     // velocidade angular (graus/s)
  float roll = 0, pitch = 0, yaw = 0; // atitude estimada (graus)
};

// Inicializa o I2C e o MPU6050. Devolve true se o sensor respondeu.
// Se devolver false, o modulo entra em "modo neutro" (leituras a 0).
bool begin();

// Atualiza as leituras e integra o filtro complementar. Chamar com regularidade
// no loop() (idealmente a >= 50 Hz). Usa o tempo real entre chamadas (dt).
void update();

// true se a IMU foi detetada e esta a fornecer dados.
bool isReady();

// Devolve a ultima leitura calculada.
const Reading& get();

} // namespace imu

#endif // DRONINHO32_IMU_H
