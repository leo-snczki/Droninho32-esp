// ============================================================================
//  Droninho32_CAM — firmware da ESP32-CAM (AI-Thinker)
//
//  Placa SEPARADA do controlador de voo (ESP32-S3). Liga-se ao Access Point do
//  drone ("Droninho32") como ESTACAO, com IP FIXO 192.168.4.2, e transmite vídeo
//  MJPEG. A app (e o S3, via /api/status) sabem onde encontrar o stream.
//
//  Endpoints (porta 80):
//    GET /        -> página de info
//    GET /stream  -> vídeo MJPEG (multipart/x-mixed-replace)
//    GET /jpg     -> uma única imagem JPEG (snapshot)
//
//  Board (Arduino IDE / arduino-cli): "AI Thinker ESP32-CAM"  (esp32:esp32:esp32cam)
//  Requer PSRAM (a AI-Thinker tem). Gravar com FTDI/USB-TTL (IO0->GND para flash).
// ============================================================================
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// ---- Rede: tem de coincidir com o AP do drone (config.h do S3) ----
static const char* AP_SSID     = "Droninho32";
static const char* AP_PASSWORD = "droninho32";
static IPAddress CAM_IP   (192, 168, 4, 2);    // IP fixo desta câmara
static IPAddress GATEWAY  (192, 168, 4, 1);    // o S3 (AP) é o gateway
static IPAddress SUBNET   (255, 255, 255, 0);

// ---- Pinos da câmara: AI-Thinker ESP32-CAM ----
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22

static httpd_handle_t s_httpd = NULL;

// Partes do protocolo MJPEG (multipart).
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=frame";
static const char* STREAM_BOUNDARY     = "\r\n--frame\r\n";
static const char* STREAM_PART         = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

// ---------------------------------------------------------------------------
//  Inicializacao da camara
// ---------------------------------------------------------------------------
static bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_VGA;       // 640x480
  config.pixel_format = PIXFORMAT_JPEG;    // necessário para streaming
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;                // 10..63 (menor = melhor qualidade)
  config.fb_count = 2;

  if (!psramFound()) {
    // Sem PSRAM: reduz para caber na RAM interna.
    config.frame_size = FRAMESIZE_QVGA;    // 320x240
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("[CAM] Falha na init: 0x%x\n", err);
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
//  Handler do stream MJPEG
// ---------------------------------------------------------------------------
static esp_err_t streamHandler(httpd_req_t* req) {
  esp_err_t res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

  char partBuf[64];
  while (true) {
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) { res = ESP_FAIL; break; }

    res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
    if (res == ESP_OK) {
      int n = snprintf(partBuf, sizeof(partBuf), STREAM_PART, fb->len);
      res = httpd_resp_send_chunk(req, partBuf, n);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char*)fb->buf, fb->len);
    }
    esp_camera_fb_return(fb);

    if (res != ESP_OK) break;   // cliente desligou
  }
  return res;
}

// ---------------------------------------------------------------------------
//  Handler de snapshot (uma imagem)
// ---------------------------------------------------------------------------
static esp_err_t jpgHandler(httpd_req_t* req) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    httpd_resp_send_500(req);
    return ESP_FAIL;
  }
  httpd_resp_set_type(req, "image/jpeg");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=droninho32.jpg");
  esp_err_t res = httpd_resp_send(req, (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
  return res;
}

// ---------------------------------------------------------------------------
//  Handler da página de info
// ---------------------------------------------------------------------------
static esp_err_t indexHandler(httpd_req_t* req) {
  const char* html =
    "<!DOCTYPE html><html><head><meta charset='utf-8'><title>Droninho32 CAM</title></head>"
    "<body style='font-family:sans-serif;text-align:center;background:#111;color:#eee'>"
    "<h2>Droninho32 - ESP32-CAM</h2>"
    "<p>Stream MJPEG:</p>"
    "<img src='/stream' style='max-width:100%'>"
    "<p><a style='color:#4fc3f7' href='/jpg'>Snapshot</a></p>"
    "</body></html>";
  httpd_resp_set_type(req, "text/html");
  return httpd_resp_send(req, html, strlen(html));
}

static void startServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.ctrl_port = 32768;

  if (httpd_start(&s_httpd, &config) == ESP_OK) {
    httpd_uri_t index = { "/",       HTTP_GET, indexHandler,  NULL };
    httpd_uri_t strm  = { "/stream", HTTP_GET, streamHandler, NULL };
    httpd_uri_t jpg   = { "/jpg",    HTTP_GET, jpgHandler,    NULL };
    httpd_register_uri_handler(s_httpd, &index);
    httpd_register_uri_handler(s_httpd, &strm);
    httpd_register_uri_handler(s_httpd, &jpg);
    Serial.println("[CAM] Servidor HTTP iniciado na porta 80.");
  } else {
    Serial.println("[CAM] Falha ao iniciar o servidor HTTP.");
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("[CAM] Droninho32_CAM a arrancar...");

  if (!initCamera()) {
    Serial.println("[CAM] Camara nao inicializada. A reiniciar em 3s...");
    delay(3000);
    ESP.restart();
  }

  // Liga ao AP do drone como estacao, com IP fixo.
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  if (!WiFi.config(CAM_IP, GATEWAY, SUBNET)) {
    Serial.println("[CAM] WiFi.config (IP fixo) falhou.");
  }
  WiFi.begin(AP_SSID, AP_PASSWORD);
  Serial.print("[CAM] A ligar ao AP \"");
  Serial.print(AP_SSID);
  Serial.print("\"");
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
    if (millis() - t0 > 20000) {   // 20 s sem ligar -> tenta de novo
      Serial.println(" timeout, a tentar de novo...");
      WiFi.disconnect();
      WiFi.begin(AP_SSID, AP_PASSWORD);
      t0 = millis();
    }
  }
  Serial.print("\n[CAM] Ligado. IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("[CAM] Stream: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/stream");

  startServer();
}

void loop() {
  // O servidor HTTP corre em tarefas próprias; se cair o WiFi, reconecta.
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[CAM] WiFi perdido. A reconectar...");
    WiFi.reconnect();
    delay(1000);
  }
  delay(500);
}
