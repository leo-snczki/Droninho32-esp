// ============================================================================
//  Droninho32 — gps.cpp
//  Implementacao da leitura do NEO-6M via TinyGPSPlus.
// ============================================================================
#include "gps.h"

#include <TinyGPSPlus.h>

namespace gps {

static TinyGPSPlus s_gps;
// Serial1 = porta UART de hardware do ESP32-S3 dedicada ao GPS (ver config.h).
static HardwareSerial s_serial(GPS_UART_NUM);
static Fix s_fix;

bool begin() {
  // SERIAL_8N1 = 8 bits, sem paridade, 1 stop bit (default do NEO-6M).
  s_serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.println(F("[GPS] UART iniciada (NEO-6M)."));
  return true;
}

// Converte a data/hora do GPS (UTC) para Unix epoch (segundos).
// Algoritmo civil de dias desde 1970 (sem libs externas).
static uint32_t toEpoch(TinyGPSDate& d, TinyGPSTime& t) {
  if (!d.isValid() || !t.isValid() || d.year() < 2020) return 0;

  int y = d.year();
  int m = d.month();
  int day = d.day();

  // Dias desde a "epoca civil" (algoritmo de Howard Hinnant, simplificado).
  y -= (m <= 2);
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yoe = (unsigned)(y - era * 400);
  unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  long days = (long)era * 146097 + (long)doe - 719468; // dias desde 1970-01-01

  return (uint32_t)(days * 86400L
                    + (long)t.hour() * 3600L
                    + (long)t.minute() * 60L
                    + (long)t.second());
}

void update() {
  // Alimenta o parser com todos os bytes disponiveis.
  while (s_serial.available() > 0) {
    s_gps.encode(s_serial.read());
  }

  // Atualiza o snapshot a partir do estado atual do parser.
  s_fix.valid = s_gps.location.isValid() && (s_gps.satellites.value() > 0);

  if (s_gps.location.isValid()) {
    s_fix.lat = s_gps.location.lat();
    s_fix.lon = s_gps.location.lng();
  }
  if (s_gps.altitude.isValid())   s_fix.alt_m      = (float)s_gps.altitude.meters();
  if (s_gps.satellites.isValid()) s_fix.sats       = (int)s_gps.satellites.value();
  if (s_gps.speed.isValid())      s_fix.speed_mps  = (float)s_gps.speed.mps();
  if (s_gps.course.isValid())     s_fix.course_deg = (float)s_gps.course.deg();

  s_fix.epoch = toEpoch(s_gps.date, s_gps.time);

  // Sem fix valido, reporta posicao neutra (mantem o resto disponivel).
  if (!s_fix.valid) {
    s_fix.lat = 0.0;
    s_fix.lon = 0.0;
  }
}

bool hasFix()        { return s_fix.valid; }
const Fix& get()     { return s_fix; }

} // namespace gps
