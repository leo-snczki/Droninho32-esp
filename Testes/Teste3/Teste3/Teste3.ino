#include <WiFi.h>
#include <WebServer.h>

const char* ssid = "ESP32_WIFI";
const char* password = "12345678";

WebServer server(80);

const int PINOS[] = {10, 11, 12, 13};
const int QTD_MOTORES = 4;

const int VEL_PARADO = 1000; 
const int VEL_GIRAR  = 1180; 

// Função direta de PWM por hardware (50Hz e 14 bits)
void enviarSinal(int pino, int microsegundos) {
  uint32_t duty = (microsegundos * 16383) / 20000;
  ledcWrite(pino, duty);
}

void enviarInterface() {
  String html = "<!DOCTYPE html><html lang='pt-BR'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Controle ESP32-S3</title>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; text-align: center; background-color: #f4f4f9; padding: 20px; }";
  html += ".container { max-width: 500px; margin: auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); }";
  html += ".btn { display: inline-block; padding: 12px 24px; margin: 10px; font-size: 16px; font-weight: bold; cursor: pointer; color: white; border-radius: 5px; border: none; }";
  html += ".btn-on { background-color: #2ecc71; }";
  html += ".btn-off { background-color: #e74c3c; }";
  html += ".grid { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin-top: 20px; }";
  html += ".card { border: 1px solid #ddd; padding: 10px; border-radius: 8px; }";
  html += "</style></head><body>";

  html += "<div class='container'><h1>Controle Geral</h1>";
  html += "<button class='btn btn-on' onclick=\"fetch('/all?state=1')\">Ligar Todos</button>";
  html += "<button class='btn btn-off' onclick=\"fetch('/all?state=0')\">Parar Todos</button><hr>";

  html += "<h3>Motores Individuais</h3><div class='grid'>";
  for (int i = 0; i < QTD_MOTORES; i++) {
    html += "<div class='card'><h4>Motor " + String(i + 1) + " (Pino " + String(PINOS[i]) + ")</h4>";
    html += "<button class='btn btn-on' onclick=\"fetch('/motor?id=" + String(i + 1) + "&state=1')\">Ligar</button>";
    html += "<button class='btn btn-off' onclick=\"fetch('/motor?id=" + String(i + 1) + "&state=0')\">Parar</button></div>";
  }
  html += "</div></div></body></html>";
  
  server.send(200, "text/html", html);
}

void tratarTodos() {
  if (server.hasArg("state")) {
    int estado = server.arg("state").toInt();
    int pulso = (estado == 1) ? VEL_GIRAR : VEL_PARADO;
    
    for (int i = 0; i < QTD_MOTORES; i++) {
      enviarSinal(PINOS[i], pulso);
    }
    
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Erro");
  }
}

void tratarIndividual() {
  if (server.hasArg("id") && server.hasArg("state")) {
    int id = server.arg("id").toInt() - 1;
    int estado = server.arg("state").toInt();
    int pulso = (estado == 1) ? VEL_GIRAR : VEL_PARADO;
    
    if (id >= 0 && id < QTD_MOTORES) {
      enviarSinal(PINOS[id], pulso);
      server.send(200, "text/plain", "OK");
    } else {
      server.send(400, "text/plain", "ID Invalido");
    }
  } else {
    server.send(400, "text/plain", "Erro");
  }
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < QTD_MOTORES; i++) {
    ledcAttach(PINOS[i], 50, 14);
    enviarSinal(PINOS[i], VEL_PARADO);
  }
  delay(2000); 

  WiFi.softAP(ssid, password);
  
  server.on("/", enviarInterface);
  server.on("/all", tratarTodos);
  server.on("/motor", tratarIndividual);

  server.begin();
  Serial.println("Pronto.");
}

void loop() {
  server.handleClient();
  delay(2);
}