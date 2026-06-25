// ============================================================================
//  Droninho32 — imu.cpp
//  Implementacao da leitura do MPU6050 e do filtro complementar.
// ============================================================================
#include "imu.h"

#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

namespace imu {

static Adafruit_MPU6050 s_mpu;
static bool      s_ready = false;
static Reading   s_data;
static uint32_t  s_lastMicros = 0;     // para calcular dt entre updates

// Constante de conversao rad -> graus.
static const float RAD_TO_DEG_F = 57.29577951308232f;

bool begin() {
  // Inicia o barramento I2C nos pinos definidos em config.h.
  // No ESP32, Wire.begin(sda, scl, freq) aceita os pinos explicitos.
  Wire.begin(IMU_I2C_SDA, IMU_I2C_SCL, IMU_I2C_FREQ);

  // Tenta detetar o sensor no endereco configurado.
  if (!s_mpu.begin(IMU_I2C_ADDR, &Wire)) {
    s_ready = false;
    Serial.println(F("[IMU] MPU6050 nao encontrado — telemetria em modo neutro."));
    return false;
  }

  // Configuracao tipica para um drone: gamas amplas e filtro passa-baixo medio.
  s_mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  s_mpu.setGyroRange(MPU6050_RANGE_500_DEG);
  s_mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);

  s_ready = true;
  s_lastMicros = micros();
  Serial.println(F("[IMU] MPU6050 inicializado."));
  return true;
}

void update() {
  if (!s_ready) return;

  sensors_event_t a, g, temp;
  s_mpu.getEvent(&a, &g, &temp);

  // Aceleracao: Adafruit devolve em m/s^2 -> converter para g (dividir por 9.81).
  s_data.ax = a.acceleration.x / 9.80665f;
  s_data.ay = a.acceleration.y / 9.80665f;
  s_data.az = a.acceleration.z / 9.80665f;

  // Giroscopio: Adafruit devolve em rad/s -> converter para graus/s.
  s_data.gx = g.gyro.x * RAD_TO_DEG_F;
  s_data.gy = g.gyro.y * RAD_TO_DEG_F;
  s_data.gz = g.gyro.z * RAD_TO_DEG_F;

  // dt real entre amostras (s), protegido contra valores absurdos.
  uint32_t now = micros();
  float dt = (now - s_lastMicros) / 1000000.0f;
  s_lastMicros = now;
  if (dt <= 0.0f || dt > 0.5f) dt = 0.01f;   // fallback ~100 Hz

  // --- Atitude a partir do acelerometro (referencia absoluta, ruidosa) ---
  // roll  em torno do eixo X ; pitch em torno do eixo Y.
  float rollAcc  = atan2f(s_data.ay, s_data.az) * RAD_TO_DEG_F;
  float pitchAcc = atan2f(-s_data.ax,
                          sqrtf(s_data.ay * s_data.ay + s_data.az * s_data.az)) * RAD_TO_DEG_F;

  // --- Filtro complementar: integra giroscopio + corrige com acelerometro ---
  float alpha = IMU_COMP_ALPHA;
  s_data.roll  = alpha * (s_data.roll  + s_data.gx * dt) + (1.0f - alpha) * rollAcc;
  s_data.pitch = alpha * (s_data.pitch + s_data.gy * dt) + (1.0f - alpha) * pitchAcc;

  // Yaw: sem magnetometro nao ha referencia absoluta. Integramos o giroscopio
  // (gz). Vai derivar com o tempo — aceitavel para o MVP, documentado.
  s_data.yaw += s_data.gz * dt;
  // Mantem yaw em [-180, 180] para legibilidade.
  if (s_data.yaw > 180.0f)  s_data.yaw -= 360.0f;
  if (s_data.yaw < -180.0f) s_data.yaw += 360.0f;
}

bool isReady()          { return s_ready; }
const Reading& get()    { return s_data; }

} // namespace imu
