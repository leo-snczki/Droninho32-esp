// ============================================================================
//  Droninho32 — comms.cpp
//  WiFi SoftAP + WebServer + API REST + heartbeat/failsafe + maquina de estados.
// ============================================================================
#include "comms.h"

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

#include "config.h"
#include "motors.h"
#include "telemetry.h"
#include "web_ui.h"

namespace comms {

static WebServer s_server(HTTP_PORT);
static State     s_state = State::Idle;

// Instante do ultimo comando/heartbeat valido (para o failsafe).
static uint32_t  s_lastCmdMs = 0;

// -------------------------------------------------------------------------
//  CORS — cabecalhos comuns a todas as respostas da API
// -------------------------------------------------------------------------
static void sendCors() {
  s_server.sendHeader("Access-Control-Allow-Origin", "*");
  s_server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  s_server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

static void sendJson(int code, const String& body) {
  sendCors();
  s_server.send(code, "application/json", body);
}

// Marca que chegou atividade do controlador (satisfaz o failsafe).
static void touchHeartbeat() { s_lastCmdMs = millis(); }

// -------------------------------------------------------------------------
//  Maquina de estados — transicoes auxiliares
//  Nota: as acoes "duras" sobre os motores ficam no modulo motors; aqui so
//  decidimos o estado logico coerente com o que os motores estao a fazer.
// -------------------------------------------------------------------------
static void recomputeState() {
  // Failsafe e Error sao estados "pegajosos": so saem por comando explicito
  // (arm/disarm/stop), tratados nos handlers. Aqui tratamos idle/armed/flying.
  if (s_state == State::Failsafe || s_state == State::Error) return;

  if (!motors::isArmed()) {
    s_state = State::Idle;
  } else if (motors::throttleGeneral() > 0) {
    s_state = State::Flying;
  } else {
    s_state = State::Armed;
  }
}

// -------------------------------------------------------------------------
//  Handlers HTTP
// -------------------------------------------------------------------------

// GET /  — pagina de controlo (HTML em PROGMEM).
static void handleRoot() {
  sendCors();
  s_server.send_P(200, "text/html", DRONINHO32_INDEX_HTML);
}

// GET /api/status
static void handleStatus() {
  sendJson(200, telemetry::buildStatusJson(apIp()));
}

// GET /api/telemetry
static void handleTelemetry() {
  sendJson(200, telemetry::buildTelemetryJson());
}

// OPTIONS /api/*  — pre-flight CORS, responde 204 sem corpo.
static void handleOptions() {
  sendCors();
  s_server.send(204);
}

// Resposta de erro JSON uniforme.
static void replyError(int code, const char* msg) {
  JsonDocument d;
  d["ok"] = false;
  d["error"] = msg;
  String out; serializeJson(d, out);
  sendJson(code, out);
}

// Resposta de sucesso JSON: { ok:true, status:"<estado>" }
static void replyOk() {
  recomputeState();
  JsonDocument d;
  d["ok"] = true;
  d["status"] = stateName();
  String out; serializeJson(d, out);
  sendJson(200, out);
}

// POST /api/command  — parse e despacho dos comandos do contrato (seccao 3).
static void handleCommand() {
  // O corpo vem em "plain" no WebServer do ESP32.
  String body = s_server.hasArg("plain") ? s_server.arg("plain") : String();

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    replyError(400, "json invalido");
    return;
  }

  const char* cmd = doc["cmd"] | "";
  if (strlen(cmd) == 0) {
    replyError(400, "campo 'cmd' em falta");
    return;
  }

  // Qualquer comando valido conta como sinal de vida (alimenta o failsafe).
  touchHeartbeat();

  // ---- heartbeat: so mantem o failsafe satisfeito ----
  if (strcmp(cmd, "heartbeat") == 0) {
    // Se estavamos em failsafe e voltou a comunicacao, ficamos em idle
    // (continua desarmado — exige novo "arm" explicito para seguranca).
    if (s_state == State::Failsafe) s_state = State::Idle;
    replyOk();
    return;
  }

  // ---- stop: PARAGEM DE EMERGENCIA imediata (qualquer estado) ----
  if (strcmp(cmd, "stop") == 0) {
    motors::emergencyStop();
    s_state = State::Idle;     // sai de failsafe/error; fica desarmado
    replyOk();
    return;
  }

  // ---- arm ----
  if (strcmp(cmd, "arm") == 0) {
    // Sai de failsafe ao armar explicitamente.
    if (s_state == State::Failsafe || s_state == State::Error) s_state = State::Idle;
    if (motors::arm()) {
      s_state = State::Armed;
      replyOk();
    } else {
      replyError(409, "arm recusado: throttle tem de estar a 0");
    }
    return;
  }

  // ---- disarm ----
  if (strcmp(cmd, "disarm") == 0) {
    motors::disarm();
    s_state = State::Idle;
    replyOk();
    return;
  }

  // ---- throttle: acelerador geral 0..100 ----
  if (strcmp(cmd, "throttle") == 0) {
    if (!doc["value"].is<int>()) { replyError(400, "throttle: 'value' em falta"); return; }
    int v = doc["value"].as<int>();
    motors::setThrottleAll(v);
    replyOk();
    return;
  }

  // ---- motor individual: {motor:0..3, value:0..100} ----
  if (strcmp(cmd, "motor") == 0) {
    if (!doc["motor"].is<int>() || !doc["value"].is<int>()) {
      replyError(400, "motor: 'motor' e 'value' obrigatorios"); return;
    }
    int idx = doc["motor"].as<int>();
    int v   = doc["value"].as<int>();
    if (idx < 0 || idx >= MOTOR_COUNT) { replyError(400, "motor fora de [0,3]"); return; }
    motors::setMotor((uint8_t)idx, v);
    replyOk();
    return;
  }

  // ---- rc: mistura de voo {roll,pitch,yaw (-100..100), throttle (0..100)} ----
  if (strcmp(cmd, "rc") == 0) {
    int roll     = doc["roll"]     | 0;
    int pitch    = doc["pitch"]    | 0;
    int yaw      = doc["yaw"]      | 0;
    int throttle = doc["throttle"] | 0;
    motors::setRC(roll, pitch, yaw, throttle);
    replyOk();
    return;
  }

  replyError(400, "comando desconhecido");
}

// 404 generico (com CORS, para o browser nao reclamar em debugging).
static void handleNotFound() {
  // Trata pre-flight OPTIONS para qualquer rota desconhecida tambem.
  if (s_server.method() == HTTP_OPTIONS) { handleOptions(); return; }
  replyError(404, "rota nao encontrada");
}

// -------------------------------------------------------------------------
//  API publica do modulo
// -------------------------------------------------------------------------

void begin() {
  // Configura o IP fixo do AP (192.168.4.1) antes de iniciar o SoftAP.
  IPAddress ip(AP_IP_0, AP_IP_1, AP_IP_2, AP_IP_3);
  IPAddress gw(AP_IP_0, AP_IP_1, AP_IP_2, AP_IP_3);
  IPAddress mask(255, 255, 255, 0);

  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(ip, gw, mask);
  WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CONN);

  Serial.print(F("[WiFi] AP \"")); Serial.print(AP_SSID);
  Serial.print(F("\" em http://")); Serial.println(WiFi.softAPIP());

  // Rotas da API (contrato seccao 3).
  s_server.on("/",              HTTP_GET,     handleRoot);
  s_server.on("/api/status",    HTTP_GET,     handleStatus);
  s_server.on("/api/telemetry", HTTP_GET,     handleTelemetry);
  s_server.on("/api/command",   HTTP_POST,    handleCommand);
  // Pre-flight CORS para os endpoints da API.
  s_server.on("/api/status",    HTTP_OPTIONS, handleOptions);
  s_server.on("/api/telemetry", HTTP_OPTIONS, handleOptions);
  s_server.on("/api/command",   HTTP_OPTIONS, handleOptions);
  s_server.onNotFound(handleNotFound);

  s_server.begin();

  // Arranque seguro: estado idle e relogio do failsafe inicializado.
  s_state = State::Idle;
  touchHeartbeat();
  Serial.println(F("[HTTP] Servidor iniciado na porta 80."));
}

void handle() {
  s_server.handleClient();
}

void update() {
  // Failsafe por heartbeat (contrato 7.4): se passou demasiado tempo sem
  // qualquer comando E estamos armados, desarma e entra em failsafe.
  uint32_t now = millis();
  if (motors::isArmed() && (now - s_lastCmdMs) > FAILSAFE_MS) {
    motors::disarm();                 // corta motores ao minimo
    s_state = State::Failsafe;
    Serial.println(F("[FAILSAFE] Sem heartbeat — motores desarmados."));
    return;
  }
  // Caso normal: mantem o estado coerente com motors.
  recomputeState();
}

State       getState() { return s_state; }

const char* stateName() {
  switch (s_state) {
    case State::Idle:     return "idle";
    case State::Armed:    return "armed";
    case State::Flying:   return "flying";
    case State::Failsafe: return "failsafe";
    case State::Error:    return "error";
  }
  return "idle";
}

String apIp() { return WiFi.softAPIP().toString(); }

int rssiDbm() {
  // Em modo SoftAP nao existe RSSI de uplink; reportamos 0 (valor neutro,
  // permitido pelo contrato). Mantido aqui para a app ler sempre o campo.
  return 0;
}

} // namespace comms
