// ============================================================================
//  Droninho32 — gps.h
//  Leitura do modulo GPS NEO-6M via UART (Serial1) usando TinyGPSPlus.
//  Expoe fix, lat, lon, alt, sats, speed, course e epoch (para o campo "ts").
//
//  MVP: o GPS e OPCIONAL. Sem fix (ou sem modulo ligado), os campos reportam
//  valores neutros (0 / false) — contrato seccao 2 e 9.
// ============================================================================
#ifndef DRONINHO32_GPS_H
#define DRONINHO32_GPS_H

#include <Arduino.h>
#include "config.h"

namespace gps {

// Snapshot dos dados de navegacao (unidades conforme contrato seccao 2).
struct Fix {
  bool     valid     = false;   // true quando ha solucao de posicao valida
  double   lat       = 0.0;     // graus decimais (WGS84)
  double   lon       = 0.0;     // graus decimais (WGS84)
  float    alt_m     = 0.0f;    // altitude (m acima do nivel do mar)
  int      sats      = 0;       // numero de satelites
  float    speed_mps = 0.0f;    // velocidade sobre o solo (m/s)
  float    course_deg= 0.0f;    // rumo (graus, 0..360)
  uint32_t epoch     = 0;       // Unix epoch (s) da data/hora do GPS; 0 sem fix
};

// Inicializa a UART do GPS. Devolve sempre true (a ausencia de fix nao e erro).
bool begin();

// Le e processa os bytes disponiveis na UART. Chamar com frequencia no loop().
void update();

// true se o modulo tem uma posicao valida e recente.
bool hasFix();

// Devolve o ultimo snapshot calculado.
const Fix& get();

} // namespace gps

#endif // DRONINHO32_GPS_H
