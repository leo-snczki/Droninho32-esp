// ============================================================================
//  Droninho32 — motors.cpp
//  Implementacao do controlo dos 4 ESC.
// ============================================================================
#include "motors.h"

namespace motors {

// Estado interno do modulo --------------------------------------------------
static bool  s_armed = false;             // true = motores podem girar
static int   s_throttleGeneral = 0;       // acelerador geral pedido (0..100)

// Potencia ALVO e EFETIVA de cada motor (0..100). A efetiva persegue a alvo
// atraves da rampa em update(), para evitar saltos bruscos de RPM.
static int   s_target[MOTOR_COUNT]  = { 0, 0, 0, 0 };
static int   s_current[MOTOR_COUNT] = { 0, 0, 0, 0 };

// -------------------------------------------------------------------------
//  Helpers internos
// -------------------------------------------------------------------------

// Satura um valor de acelerador a [0, THROTTLE_MAX_TEST].
static int clampThrottle(int pct) {
  if (pct < 0) pct = 0;
  if (pct > THROTTLE_MAX_TEST) pct = THROTTLE_MAX_TEST;
  return pct;
}

// Escreve diretamente o pulso minimo em todos os canais (sem rampa).
// Usado no arranque e em paragens de emergencia.
static void writeAllMin() {
  uint32_t duty = usToDuty(ESC_MIN_US);
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    ledcWrite(MOTOR_PINS[i], duty);
  }
}

// Zera alvos e potencia efetiva (estado parado coerente).
static void zeroTargets() {
  s_throttleGeneral = 0;
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    s_target[i]  = 0;
    s_current[i] = 0;
  }
}

// -------------------------------------------------------------------------
//  API publica
// -------------------------------------------------------------------------

void begin() {
  // Anexa cada pino a um canal ledc (50 Hz, 14 bits) e poe no minimo.
  // No core ESP32 v3.x, ledcAttach(pino, freq, res) trata da alocacao do canal.
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    ledcAttach(MOTOR_PINS[i], PWM_FREQ_HZ, PWM_RES_BITS);
  }
  s_armed = false;
  zeroTargets();
  writeAllMin();   // SEGURANCA: nunca girar no boot (contrato 7.1)
}

bool arm() {
  // SEGURANCA: so arma com o acelerador a 0 (contrato 7.3).
  if (s_throttleGeneral != 0) {
    return false;
  }
  zeroTargets();
  s_armed = true;
  return true;
}

void disarm() {
  s_armed = false;
  zeroTargets();
  writeAllMin();
}

void emergencyStop() {
  // Identico a disarm, mas semanticamente "corte imediato".
  s_armed = false;
  zeroTargets();
  writeAllMin();
}

void setThrottleAll(int pct) {
  pct = clampThrottle(pct);
  s_throttleGeneral = pct;
  // Sem mistura: todos os motores partilham o acelerador geral.
  // Se desarmado, mantem alvo 0 (a potencia so sobe quando armar).
  int t = s_armed ? pct : 0;
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    s_target[i] = t;
  }
}

void setMotor(uint8_t index, int pct) {
  if (index >= MOTOR_COUNT) return;
  pct = clampThrottle(pct);
  // So atua se armado (contrato 7.3). Desarmado -> ignora (mantem 0).
  s_target[index] = s_armed ? pct : 0;
}

void setRC(int roll, int pitch, int yaw, int throttle) {
  // Throttle base saturado ao limite de teste.
  int base = clampThrottle(throttle);
  s_throttleGeneral = base;

  if (!s_armed) {
    // Desarmado: nao mexe nos motores (alvos a 0).
    for (uint8_t i = 0; i < MOTOR_COUNT; i++) s_target[i] = 0;
    return;
  }

  // Normaliza entradas -100..100 e escala pela autoridade (em % de potencia).
  if (roll  < -100) roll  = -100; if (roll  > 100) roll  = 100;
  if (pitch < -100) pitch = -100; if (pitch > 100) pitch = 100;
  if (yaw   < -100) yaw   = -100; if (yaw   > 100) yaw   = 100;

  // Cada eixo contribui ate MIX_AUTHORITY_PCT de potencia.
  float r = (roll  / 100.0f) * MIX_AUTHORITY_PCT;   // roll  > 0 -> rola para a direita
  float p = (pitch / 100.0f) * MIX_AUTHORITY_PCT;   // pitch > 0 -> nariz para cima
  float y = (yaw   / 100.0f) * MIX_AUTHORITY_PCT;   // yaw   > 0 -> roda no sentido horario

  // Mistura quadcoptero em X (ordem FL, FR, RL, RR).
  // Convencao: motores FL e RR giram num sentido, FR e RL no outro (yaw).
  //   roll  positivo -> aumenta motores da esquerda (FL, RL)
  //   pitch positivo -> aumenta motores de tras (RL, RR)
  //   yaw   positivo -> aumenta o par FL+RR
  float mFL = base + r + p + y;
  float mFR = base - r + p - y;
  float mRL = base + r - p - y;
  float mRR = base - r - p + y;

  float mix[MOTOR_COUNT] = { mFL, mFR, mRL, mRR };
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    int v = (int)lround(mix[i]);
    s_target[i] = clampThrottle(v);   // satura cada motor a [0, THROTTLE_MAX_TEST]
  }
}

void update() {
  // SEGURANCA: se desarmado, garante minimo e sai.
  if (!s_armed) {
    writeAllMin();
    return;
  }

  // Rampa: aproxima a potencia efetiva da alvo em passos de THROTTLE_RAMP_STEP.
  for (uint8_t i = 0; i < MOTOR_COUNT; i++) {
    int delta = s_target[i] - s_current[i];
    if (delta > THROTTLE_RAMP_STEP)       s_current[i] += THROTTLE_RAMP_STEP;
    else if (delta < -THROTTLE_RAMP_STEP) s_current[i] -= THROTTLE_RAMP_STEP;
    else                                  s_current[i]  = s_target[i];

    // Converte a potencia efetiva (0..100) em duty e escreve no ledc.
    ledcWrite(MOTOR_PINS[i], throttlePctToDuty(s_current[i]));
  }
}

bool isArmed()                 { return s_armed; }
int  throttleGeneral()         { return s_throttleGeneral; }
int  motorPercent(uint8_t i)   { return (i < MOTOR_COUNT) ? s_current[i] : 0; }

} // namespace motors
