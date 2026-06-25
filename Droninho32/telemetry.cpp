// ============================================================================
//  Droninho32 — telemetry.cpp
//  Monta o JSON TelemetryPoint (contrato seccao 2) com ArduinoJson v7.
// ============================================================================
#include "telemetry.h"

#include <ArduinoJson.h>   // v7: usa JsonDocument (sem tamanho fixo)
#include "config.h"
#include "motors.h"
#include "imu.h"
#include "gps.h"
#include "comms.h"

namespace telemetry {

// Estima a percentagem de bateria a partir da tensao (linear, simplificado).
static int batteryPercentFromVoltage(float v) {
  if (v <= BATTERY_VMIN) return 0;
  if (v >= BATTERY_VMAX) return 100;
  return (int)lround((v - BATTERY_VMIN) / (BATTERY_VMAX - BATTERY_VMIN) * 100.0f);
}

// Le a tensao da bateria (se ativada). Sem hardware -> 0.0 (valor neutro).
static float readBatteryVoltage() {
#if BATTERY_ENABLED
  // ADC de 12 bits (0..4095) referenciado a ~3.3 V, corrigido pelo divisor.
  int raw = analogRead(BATTERY_PIN);
  float vadc = (raw / 4095.0f) * 3.3f;
  return vadc * BATTERY_DIV;
#else
  return 0.0f;
#endif
}

String buildTelemetryJson() {
  JsonDocument doc;   // ArduinoJson v7

  const gps::Fix& fix = gps::get();
  const imu::Reading& a = imu::get();

  // Campos de topo (ordem conforme o contrato) -----------------------------
  // "ts": epoch do GPS se houver fix, senao 0.
  doc["ts"]        = fix.valid ? fix.epoch : (uint32_t)0;
  doc["uptime_ms"] = (uint32_t)millis();
  doc["armed"]     = motors::isArmed();
  doc["status"]    = comms::stateName();
  doc["throttle"]  = motors::throttleGeneral();

  // "motors": potencia efetiva por motor (FL, FR, RL, RR), 0..100.
  JsonArray m = doc["motors"].to<JsonArray>();
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    m.add(motors::motorPercent(i));
  }

  // "attitude": atitude estimada (graus).
  JsonObject att = doc["attitude"].to<JsonObject>();
  att["roll"]  = a.roll;
  att["pitch"] = a.pitch;
  att["yaw"]   = a.yaw;

  // "imu": leituras cruas (a em g, g em graus/s).
  JsonObject im = doc["imu"].to<JsonObject>();
  im["ax"] = a.ax; im["ay"] = a.ay; im["az"] = a.az;
  im["gx"] = a.gx; im["gy"] = a.gy; im["gz"] = a.gz;

  // "gps": posicao e navegacao (valores neutros sem fix).
  JsonObject g = doc["gps"].to<JsonObject>();
  g["fix"]        = fix.valid;
  g["lat"]        = fix.lat;
  g["lon"]        = fix.lon;
  g["alt_m"]      = fix.alt_m;
  g["sats"]       = fix.sats;
  g["speed_mps"]  = fix.speed_mps;
  g["course_deg"] = fix.course_deg;

  // "battery": tensao + percentagem estimada (0 se nao medida).
  float vbat = readBatteryVoltage();
  JsonObject bat = doc["battery"].to<JsonObject>();
  bat["voltage"] = vbat;
  bat["percent"] = (vbat > 0.0f) ? batteryPercentFromVoltage(vbat) : 0;

  // "rssi": forca do sinal WiFi (dBm).
  doc["rssi"] = comms::rssiDbm();

  String out;
  serializeJson(doc, out);
  return out;
}

String buildStatusJson(const String& ip) {
  JsonDocument doc;
  doc["name"]      = FW_NAME;
  doc["firmware"]  = FW_VERSION;
  doc["uptime_ms"] = (uint32_t)millis();
  doc["armed"]     = motors::isArmed();
  doc["status"]    = comms::stateName();
  doc["ip"]        = ip;

  String out;
  serializeJson(doc, out);
  return out;
}

} // namespace telemetry
