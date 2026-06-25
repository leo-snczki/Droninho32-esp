// ============================================================================
//  Droninho32 — telemetry.h
//  Monta o objeto JSON "TelemetryPoint" EXATAMENTE como definido no contrato
//  (docs/ARCHITECTURE.md, seccao 2), usando ArduinoJson v7.
//
//  Le os dados dos restantes modulos (motors, imu, gps) e o estado/status
//  da maquina de estados (comms). Nao mantem estado proprio.
// ============================================================================
#ifndef DRONINHO32_TELEMETRY_H
#define DRONINHO32_TELEMETRY_H

#include <Arduino.h>

namespace telemetry {

// Constroi o JSON TelemetryPoint completo e devolve-o como String.
// Campos sempre presentes, ordem e nomes conforme o contrato.
String buildTelemetryJson();

// Constroi o JSON reduzido de /api/status:
//   { name, firmware, uptime_ms, armed, status, ip }
String buildStatusJson(const String& ip);

} // namespace telemetry

#endif // DRONINHO32_TELEMETRY_H
