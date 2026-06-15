#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

const char* ssid = "ESP32_WIFI";
const char* password = "12345678";

WebServer server(80);

Servo esc;

#define ESC_PIN 4

int throttle = 1000;

void handleRoot() {

  String html = R"rawliteral(
    <html>
    <body>
      <h1>Controle ESC</h1>

      <a href="/start">
        <button style="width:200px;height:80px;font-size:30px;">
          START
        </button>
      </a>

      <a href="/stop">
        <button style="width:200px;height:80px;font-size:30px;">
          STOP
        </button>
      </a>

    </body>
    </html>
  )rawliteral";

  server.send(200, "text/html", html);
}

void handleStart() {
  throttle = 1300;
  esc.writeMicroseconds(throttle);

  server.send(200, "text/html", "ESC LIGADO");
}

void handleStop() {
  throttle = 1000;
  esc.writeMicroseconds(throttle);

  server.send(200, "text/html", "ESC PARADO");
}

void setup() {

  Serial.begin(115200);

  esc.attach(ESC_PIN, 1000, 2000);

  esc.writeMicroseconds(1000);

  delay(5000);

  WiFi.softAP(ssid, password);

  Serial.println("WiFi iniciado");
  Serial.println(WiFi.softAPIP());

  server.on("/", handleRoot);
  server.on("/start", handleStart);
  server.on("/stop", handleStop);

  server.begin();
}

void loop() {
  server.handleClient();
}