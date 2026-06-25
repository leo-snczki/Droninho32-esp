// ============================================================================
//  Droninho32 — comms.h
//  WiFi SoftAP + WebServer(80) + API REST /api/* + heartbeat/failsafe.
//
//  Este modulo e tambem o dono da MAQUINA DE ESTADOS do drone, porque as
//  transicoes sao dirigidas pelos comandos recebidos e pelo failsafe por
//  heartbeat (ambos vivem aqui). O Droninho32.ino orquestra: chama begin(),
//  e em loop() chama handle() (servidor) + update() (failsafe/estado).
//
//  Decisao de arquitetura (registada): o contrato (seccao 6) coloca a maquina
//  de estados no .ino; mantemos a logica de transicoes aqui junto do heartbeat
//  para evitar dependencias circulares e duplicacao, e o .ino continua a ser o
//  ponto de orquestracao. O estado e exposto por getState()/stateName().
//
//  Estados: idle -> armed -> flying -> failsafe -> error  (contrato seccao 2).
// ============================================================================
#ifndef DRONINHO32_COMMS_H
#define DRONINHO32_COMMS_H

#include <Arduino.h>

namespace comms {

// Maquina de estados do drone. Os nomes textuais (stateName) seguem o contrato.
enum class State {
  Idle,       // "idle"     — desarmado, em repouso
  Armed,      // "armed"    — armado, motores ao minimo, throttle 0
  Flying,     // "flying"   — armado e com throttle > 0
  Failsafe,   // "failsafe" — sem heartbeat: desarmou por seguranca
  Error       // "error"    — falha interna (reservado)
};

// Inicializa o SoftAP (SSID/pass/IP do config.h) e regista as rotas HTTP.
void begin();

// Processa pedidos HTTP pendentes. Chamar a alta frequencia no loop().
void handle();

// Atualiza a maquina de estados e verifica o failsafe por heartbeat.
// Chamar regularmente no loop().
void update();

// Estado atual e o seu nome textual (para telemetria/status).
State        getState();
const char*  stateName();

// IP do AP como string (ex.: "192.168.4.1") — usado no /api/status.
String apIp();

// RSSI do WiFi em dBm (em modo AP nao ha um RSSI proprio do uplink; reportamos
// 0 conforme o contrato permite valores neutros). Mantido para clareza.
int rssiDbm();

} // namespace comms

#endif // DRONINHO32_COMMS_H
