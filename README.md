# Droninho32 — Firmware (ESP32-S3)

Firmware unificado e modular do drone **Droninho32**, escrito em **C++ (Arduino core para ESP32)**.

> ⚠️ **Aviso de segurança (ler primeiro):** este é um drone experimental. Os motores brushless
> 1400KV com hélices podem causar ferimentos graves. **Testa SEMPRE sem hélices** até teres a
> certeza de que tudo funciona, e sobe o acelerador devagar e à mão. Vê a [Checklist de
> Segurança](#checklist-de-segurança-obrigatório) antes de cada teste.

O Droninho32 é uma iniciativa de criar um **veículo aéreo não tripulado (VANT)** com uma
arquitetura **modular e de código aberto**, ao contrário das soluções comerciais fechadas. Este
submódulo é o **cérebro embarcado**: corre no microcontrolador ESP32-S3, cria um **Access Point
WiFi** próprio e expõe uma **API REST/JSON** para controlo dos 4 motores, leitura dos sensores de
movimento (IMU) e de localização (GPS), e envio de telemetria em tempo real. É a peça que une o
mundo físico (motores, sensores) ao mundo digital (app Android e backend Django).

O firmware implementa exatamente o contrato definido em
[`docs/ARCHITECTURE.md`](../docs/ARCHITECTURE.md) (secções 2, 3, 7 e 8), garantindo que os JSON
produzidos aqui são consumidos sem ambiguidade pela app e pelo backend.

---

## Índice

1. [Descrição](#1-descrição)
2. [Funcionalidades](#2-funcionalidades)
3. [Hardware suportado](#3-hardware-suportado)
4. [Estrutura do projeto](#4-estrutura-do-projeto)
5. [Bibliotecas necessárias](#5-bibliotecas-necessárias)
6. [Pinout e esquema de ligações](#6-pinout-e-esquema-de-ligações)
7. [Compilar e carregar](#7-compilar-e-carregar)
8. [Parâmetros de configuração (`config.h`)](#8-parâmetros-de-configuração-configh)
9. [Calibração dos ESC](#9-calibração-dos-esc-sem-hélices)
10. [Checklist de segurança](#checklist-de-segurança-obrigatório)
11. [API do drone](#11-api-do-drone)
12. [Máquina de estados e failsafe](#12-máquina-de-estados-e-failsafe)
13. [ESP32-CAM (vídeo)](#13-esp32-cam-vídeo)
14. [Resolução de problemas](#14-resolução-de-problemas)
15. [Como estender](#15-como-estender)
16. [Feedback e suporte](#16-feedback-e-suporte)
17. [Licença](#17-licença)

---

## 1. Descrição

A maioria dos sketches Arduino tem **um único** `.ino`. Aqui o sketch é **unificado mas modular**:
o `Droninho32.ino` orquestra vários módulos `.h/.cpp` na **mesma pasta**, que o Arduino IDE compila
automaticamente (cada `.cpp` torna-se uma *translation unit*; os `namespace` evitam colisões de
nomes). Esta organização separa responsabilidades — motores, IMU, GPS, telemetria, comunicações —
mantendo a manutenção e a evolução simples.

Ao ligar, o firmware:

1. arranca em estado seguro (`idle`, motores no pulso mínimo de 1000 µs — **não giram**);
2. cria o **Access Point WiFi** `Droninho32` em `192.168.4.1`;
3. inicializa os sensores (MPU6050 por I2C, NEO-6M por UART);
4. começa a servir a **API REST** e a página de controlo manual;
5. monitoriza o **heartbeat** da app — se falhar, ativa o **failsafe** e desarma sozinho.

---

## 2. Funcionalidades

- **Access Point WiFi autónomo** — o drone não precisa de router; cria a sua própria rede.
- **API REST/JSON** — `/api/status`, `/api/telemetry`, `/api/command` (contrato §3).
- **Controlo dos 4 motores** via ESC, com PWM por hardware (`ledc`, 50 Hz, 14 bits).
- **Mistura quad-X** — comando `rc` (roll/pitch/yaw/throttle) distribuído pelos 4 motores.
- **Rampa de aceleração** — sobe a potência suavemente (sem saltos bruscos de RPM).
- **Leitura da IMU (MPU6050)** com **filtro complementar** → atitude (roll/pitch/yaw).
- **Leitura do GPS (NEO-6M)** via TinyGPSPlus → lat/lon/altitude/satélites/velocidade/rumo/epoch.
- **Telemetria canónica** (`TelemetryPoint`, ArduinoJson v7) idêntica em todo o sistema.
- **Máquina de estados** `idle → armed → flying`, com `failsafe` e `error`.
- **Failsafe por heartbeat** — desarma sozinho se perder a ligação à app.
- **Paragem de emergência** (`stop`) — corta os motores imediatamente em qualquer estado.
- **UI web de controlo manual** embebida (HTML em PROGMEM).
- **Anúncio da ESP32-CAM** — publica o `camera_url` em `/api/status`.

---

## 3. Hardware suportado

| Componente | Especificação | Função |
|------------|---------------|--------|
| **ESP32-S3** (com placa de ensaio) | Dual Core 240 MHz, 8 MB PSRAM, 16 MB Flash | Unidade de processamento e controlo de voo |
| **GY-521 (MPU-6050)** | Acelerómetro + giroscópio (I2C) | Estabilização e leitura de inclinação/orientação |
| **GPS NEO-6M** | Módulo de geolocalização (UART) | Coordenadas, altitude e velocidade |
| **4× Motor Brushless** | 1400 KV | Propulsão e sustentação |
| **4× ESC** | Entrada XT60, máx. 40 A | Controlo da velocidade de cada motor |
| **4–5× Hélices** | Tamanho 7045 | Conversão da rotação em empuxo |
| **Bateria LiPo** | 3S (11,1 V) 2200 mAh 60C | Alimentação principal |
| **Antena WiFi** | Ganho para maior alcance | Amplia a distância de transmissão |
| **ESP-CAM** | Módulo de câmara com WiFi | Captura/transmissão de vídeo (placa separada) |

### Especificações reais da placa (lidas com `esptool.py`)

A placa adquirida foi inspecionada por `esptool.py` (porta `COM8`):

| Parâmetro | Valor detetado |
|-----------|----------------|
| Tipo de chip | ESP32-S3 (QFN56), revisão v0.2 |
| Recursos | WiFi, BT 5 (LE), Dual Core + LP Core, 240 MHz, PSRAM 8 MB |
| Frequência do cristal | 40 MHz |
| Endereço MAC | `DC:B4:D9:0F:8C:C8` |
| Flash detetada | 16 MB (modo *quad*, 4 data lines) |
| Tensão da flash (eFuse) | 3,3 V |

---

## 4. Estrutura do projeto

```
droninho32-esp/
├─ Droninho32/                 # Sketch Arduino UNIFICADO — abrir ESTA pasta no Arduino IDE
│  ├─ Droninho32.ino           # setup()/loop(): orquestra os módulos e a máquina de estados
│  ├─ config.h                 # pinos, PWM, WiFi, sensores, limites de segurança (FONTE DA VERDADE)
│  ├─ motors.h / motors.cpp    # ESC via ledc (50 Hz/14 bit), arm/disarm, mistura quad-X, rampa
│  ├─ imu.h / imu.cpp          # MPU6050 (Adafruit) + filtro complementar (roll/pitch/yaw)
│  ├─ gps.h / gps.cpp          # NEO-6M via TinyGPSPlus (lat/lon/alt/sats/speed/course/epoch)
│  ├─ telemetry.h / .cpp       # monta o JSON TelemetryPoint (ArduinoJson v7)
│  ├─ comms.h / comms.cpp      # SoftAP + WebServer + API /api/* + heartbeat/failsafe + estados
│  └─ web_ui.h                 # HTML (PROGMEM) da página de controlo manual
├─ Droninho32_CAM/             # Sketch SEPARADO da ESP32-CAM (stream MJPEG) — ver §13
├─ Testes/                     # protótipos anteriores (Teste1/2/3) — mantidos como referência
├─ specs ESP32S3.txt           # info da placa (chip, flash, MAC)
├─ LICENSE.md                  # PolyForm Noncommercial 1.0.0
└─ README.md                   # este ficheiro
```

---

## 5. Bibliotecas necessárias

Instalar pelo **Library Manager** do Arduino IDE (*Sketch → Include Library → Manage Libraries…*)
ou via `arduino-cli lib install`:

| Biblioteca | Versão | Porquê |
|------------|--------|--------|
| **esp32** (Arduino core, Espressif) | **3.x** | placa ESP32-S3, `ledcAttach`/`ledcWrite`, `WiFi`, `WebServer` |
| **Adafruit MPU6050** | ^2.2 | driver do MPU6050 (GY-521) |
| **Adafruit Unified Sensor** | ^1.1 | dependência do anterior |
| **TinyGPSPlus** (Mikal Hart) | ^1.0.3 | parsing NMEA do NEO-6M |
| **ArduinoJson** (Benoit Blanchon) | **^7.0** | serialização do TelemetryPoint |

```bash
# Core ESP32 (uma vez):
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32

# Bibliotecas:
arduino-cli lib install "Adafruit MPU6050" "Adafruit Unified Sensor" "TinyGPSPlus" "ArduinoJson"
```

> **ArduinoJson v7:** este código usa `JsonDocument` (sem capacidade fixa) e a API v7
> (`doc["x"].to<JsonArray>()`, `.is<int>()`, `value | default`). **Não funciona com a v6.**

---

## 6. Pinout e esquema de ligações

Todos os pinos são configuráveis em `config.h`. Por defeito:

| Função | Pino ESP32-S3 | Notas |
|--------|---------------|-------|
| Motor **FL** (índice 0) | **GPIO10** | ESC, sinal PWM 50 Hz |
| Motor **FR** (índice 1) | **GPIO11** | ESC |
| Motor **RL** (índice 2) | **GPIO12** | ESC |
| Motor **RR** (índice 3) | **GPIO13** | ESC |
| **MPU6050 SDA** (I2C) | **GPIO8** | default do `Wire` no ESP32-S3 |
| **MPU6050 SCL** (I2C) | **GPIO9** | default do `Wire` no ESP32-S3 |
| **NEO-6M → RX** (UART1) | **GPIO18** | ESP **RX** ← **TX** do GPS |
| **NEO-6M → TX** (UART1) | **GPIO17** | ESP **TX** → **RX** do GPS (pouco usado) |
| Bateria (ADC, opcional) | GPIO4 | desligado por defeito (`BATTERY_ENABLED 0`) |

> A ordem **FL, FR, RL, RR** é a convenção fixa do contrato (secção 2). Se os teus ESC estiverem
> noutros pinos, muda só o array `MOTOR_PINS` em `config.h`.

```
                 ┌───────────────────────────┐
   MPU6050/GY-521│ VCC→3V3  GND→GND           │
   (I2C)         │ SDA→GPIO8   SCL→GPIO9      │
                 │                            │
   NEO-6M (GPS)  │ VCC→3V3/5V*  GND→GND       │   * conforme o teu módulo
   (UART)        │ TX →GPIO18 (ESP RX)        │
                 │ RX ←GPIO17 (ESP TX)        │       ESP32-S3
                 │                            │
   ESC ×4        │ sinal→GPIO10/11/12/13      │
   (motores)     │ GND comum com o ESP        │
                 │ alimentação dos ESC pela   │
                 │ bateria 3S (NÃO pelo USB)  │
                 └───────────────────────────┘
```

> **Massa comum:** o GND dos ESC, do GPS, da IMU e do ESP têm de estar ligados.
> **Alimentação dos motores** vem da bateria 3S (via ESC), **nunca** do USB do PC.
> O diagrama completo de ligações foi desenhado em **Fritzing** (ver relatório da PAP).

### Disposição quad-X dos motores

```
      FL (0)        FR (1)         frente
          \        /
           \      /
           ESP32-S3
           /      \
          /        \
      RL (2)        RR (3)         trás
```

---

## 7. Compilar e carregar

A placa é **ESP32-S3** (FQBN `esp32:esp32:esp32s3`). A porta vista nas specs é `COM8` (ajusta à tua).

```bash
# Compilar
arduino-cli compile --fqbn esp32:esp32:esp32s3 Droninho32

# Carregar (ajusta a porta COM)
arduino-cli upload  --fqbn esp32:esp32:esp32s3 -p COM8 Droninho32

# Monitor série (115200 baud em todos os sketches)
arduino-cli monitor -p COM8 -b 115200
```

**Em alternativa (Arduino IDE):** abrir a pasta `Droninho32/`, selecionar a placa *ESP32S3 Dev
Module*, *Upload Speed* 115200, e carregar com **Upload ▶**.

---

## 8. Parâmetros de configuração (`config.h`)

`config.h` é a **fonte da verdade** do firmware — nenhum módulo tem "números mágicos". Principais
parâmetros:

| Constante | Valor | Significado |
|-----------|-------|-------------|
| `FW_VERSION` | `1.0.0` | versão reportada na telemetria/status |
| `MOTOR_PINS` | `{10,11,12,13}` | pinos PWM dos ESC (FL, FR, RL, RR) |
| `PWM_FREQ_HZ` / `PWM_RES_BITS` | `50` / `14` | frequência e resolução do `ledc` |
| `ESC_MIN_US` / `ESC_MAX_US` | `1000` / `2000` | pulsos mínimo/máximo dos ESC (µs) |
| `THROTTLE_MAX_TEST` | `100` | saturação do acelerador em % — **testar sempre sem hélices!** |
| `THROTTLE_RAMP_STEP` | `2` | variação máx. de potência por ciclo (rampa) |
| `MOTOR_UPDATE_MS` | `20` | período de atualização dos motores (50 Hz) |
| `FAILSAFE_MS` | `1500` | tempo sem heartbeat até desarmar |
| `MIX_AUTHORITY_PCT` | `20` | autoridade máx. de roll/pitch/yaw na mistura |
| `AP_SSID` / `AP_PASSWORD` | `Droninho32` / `droninho32` | rede WiFi do drone (password ≥ 8 chars) |
| `IMU_COMP_ALPHA` | `0.98` | peso do giroscópio no filtro complementar |
| `BATTERY_ENABLED` | `0` | medição de bateria desligada por defeito |

---

## 9. Calibração dos ESC (sem hélices!)

Os ESC precisam de aprender os pulsos mínimo/máximo. Faz isto **uma vez** por ESC (ou todos
juntos), **sem hélices montadas**:

1. **Não** ligues a bateria ainda.
2. Envia o **pulso máximo** (2000 µs) — num teste de bancada controlado.
3. Liga a bateria. O ESC emite os *beeps* de "throttle máximo detetado".
4. Baixa o sinal para o **pulso mínimo** (1000 µs). O ESC confirma com *beeps*.
5. O ESC fica calibrado. Desliga e volta ao firmware normal.

> O firmware normal arranca já no **mínimo (1000 µs)**, por isso, depois de calibrado, basta ligar
> — os motores ficam parados até `arm` + `throttle`.

---

## Checklist de SEGURANÇA (obrigatório)

Antes de **cada** teste:

- [ ] **Hélices removidas** durante todos os testes de software.
- [ ] `THROTTLE_MAX_TEST` consciente (está a 100% por opção) → **só testar SEM hélices** e subir o acelerador devagar.
- [ ] Alimentação dos motores pela **bateria**; o ESP pelo USB só para debug.
- [ ] Confirmar no boot: estado `idle`, motores no mínimo, **não giram**.
- [ ] Testar `arm` → motores continuam parados (throttle 0).
- [ ] Subir `throttle` devagar e confirmar a **rampa** (sem saltos).
- [ ] Testar `stop` — corte imediato.
- [ ] Testar **failsafe**: parar de enviar heartbeat → desarma sozinho em ~1,5 s.
- [ ] Usar **óculos de proteção (EPI)** ao aproximar das hélices.
- [ ] Só montar hélices depois de tudo isto validado, em ambiente seguro e aberto.

---

## 11. API do drone

Servidor HTTP em **`http://192.168.4.1`** (porta 80). Respostas JSON, CORS `*`.

| Método | Rota | Descrição |
|--------|------|-----------|
| `GET` | `/` | UI web de controlo manual (HTML) |
| `GET` | `/api/status` | `{ name, firmware, uptime_ms, armed, status, ip, camera_url }` |
| `GET` | `/api/telemetry` | objeto **TelemetryPoint** (contrato secção 2) |
| `POST`| `/api/command` | enviar comando (ver abaixo) → `{ ok, status }` |
| `OPTIONS` | `/api/*` | pré-flight CORS → `204` |

**Comandos** (`POST /api/command`, corpo JSON, campo `cmd`):

```jsonc
{ "cmd": "arm" }                                 // arma (exige throttle a 0)
{ "cmd": "disarm" }                              // desarma
{ "cmd": "stop" }                                // PARAGEM DE EMERGÊNCIA
{ "cmd": "throttle", "value": 35 }               // acelerador geral 0..100
{ "cmd": "motor", "motor": 2, "value": 40 }      // motor individual (0..3), só armado
{ "cmd": "rc", "roll":0, "pitch":0, "yaw":0, "throttle":30 } // mistura de voo
{ "cmd": "heartbeat" }                            // mantém o failsafe satisfeito (~2 Hz)
```

**Exemplos `curl`** (depois de ligar o PC ao WiFi `Droninho32` / `droninho32`):

```bash
curl http://192.168.4.1/api/status
curl http://192.168.4.1/api/telemetry
curl -X POST http://192.168.4.1/api/command -H "Content-Type: application/json" -d '{"cmd":"arm"}'
curl -X POST http://192.168.4.1/api/command -H "Content-Type: application/json" -d '{"cmd":"throttle","value":20}'
curl -X POST http://192.168.4.1/api/command -H "Content-Type: application/json" -d '{"cmd":"stop"}'
```

---

## 12. Máquina de estados e failsafe

```
        arm (throttle=0)            throttle>0
  idle ───────────────► armed ───────────────► flying
   ▲  ◄─────────────── │  ◄─────────────────── │
   │      disarm        │     throttle=0        │
   │                    │                       │
   │   stop (qualquer estado) ──────────────────┘
   │                    │
   └──────── failsafe ◄─┘  (sem heartbeat > FAILSAFE_MS → desarma)
             (sai com arm/disarm/stop/heartbeat)
```

- **idle** — desarmado, motores no mínimo.
- **armed** — armado, throttle 0 (motores no mínimo, prontos).
- **flying** — armado com throttle > 0.
- **failsafe** — perdeu o heartbeat: desarmou por segurança. Recupera com `arm` explícito.
- **error** — reservado para falhas internas.

### Comportamento sem GPS / sem bateria (MVP)

O firmware funciona mesmo **sem GPS e sem medição de bateria** ligados:

- **IMU (MPU6050):** se não responder, a telemetria reporta atitude/aceleração a `0`. É o sensor central do MVP — recomenda-se tê-la sempre ligada.
- **GPS (NEO-6M):** sem fix, `gps.fix=false` e `lat/lon=0`. O drone controla-se na mesma.
- **Bateria:** `BATTERY_ENABLED 0` por defeito → `voltage=0`, `percent=0`.

Assim, **controlo manual + telemetria de atitude** funcionam de imediato.

---

## 13. ESP32-CAM (vídeo)

A câmara é uma **placa separada** (`Droninho32_CAM/`) que se liga ao AP `Droninho32` como **estação**
em IP fixo **192.168.4.2** e transmite **MJPEG**:

| Recurso | URL |
|---------|-----|
| Stream de vídeo (MJPEG) | `http://192.168.4.2/stream` |
| Snapshot (imagem única) | `http://192.168.4.2/jpg` |

Compilar/gravar com a board **AI Thinker ESP32-CAM** (`esp32:esp32:esp32cam`). Para entrar em modo de
gravação, liga **IO0 → GND** na ESP32-CAM. O firmware do S3 **não** comunica diretamente com a câmara
— apenas anuncia o `camera_url` em `/api/status`, e a app mostra o vídeo no ecrã de Câmara.

---

## 14. Resolução de problemas

| Sintoma | Causa provável | Solução |
|---------|----------------|---------|
| Erro a compilar `JsonDocument` / `.to<JsonArray>()` | ArduinoJson v6 instalada | Atualiza para **ArduinoJson v7** |
| `ledcAttach` não existe | Core ESP32 v2.x | Atualiza o core **esp32 para 3.x** |
| A rede `Droninho32` não aparece | AP não arrancou / password < 8 chars | Vê o monitor série; confirma `AP_PASSWORD` ≥ 8 chars |
| Os motores não respondem | Drone não armado / throttle 0 | Envia `arm` e só depois `throttle` |
| Os motores desarmam sozinhos | Failsafe (sem heartbeat) | A app tem de enviar `heartbeat` a ~2 Hz |
| IMU reporta sempre 0 | I2C mal ligado / endereço errado | Confirma SDA=GPIO8, SCL=GPIO9, `IMU_I2C_ADDR 0x68` |
| GPS nunca tem fix | Sem vista para o céu / TX↔RX trocados | Testa no exterior; confirma TX do GPS → GPIO18 |
| ESC fazem *beeps* contínuos | Não calibrados | Faz a [calibração dos ESC](#9-calibração-dos-esc-sem-hélices) |
| Upload falha | Porta COM errada / placa em modo errado | Confirma a porta; mantém BOOT premido se necessário |

---

## 15. Como estender

- **PID / estabilização em malha fechada:** consumir `imu::get()` (roll/pitch/yaw) e corrigir os alvos em `motors::setRC` antes da mistura (futuro, contrato §9).
- **WebSocket de telemetria** (`/ws`, 5–10 Hz): adicionar em `comms.cpp`; o MVP usa *polling* a `/api/telemetry`.
- **Medição de bateria:** ligar um divisor resistivo ao ADC e pôr `BATTERY_ENABLED 1`.
- **Mudar pinos/limites:** tudo em `config.h` — nenhum módulo tem números mágicos.

---

## 16. Feedback e suporte

Desenvolvido por: **Leonardo Zelenski** — Prova de Aptidão Profissional (PAP), 12.º GPSI B.

📧 Contacto: [leonardo@munit.pt](mailto:leonardo@munit.pt)

Para reportar bugs ou sugerir funcionalidades, abre uma *issue* no GitHub.

---

## 17. Licença

Este projeto está licenciado sob a **PolyForm Noncommercial License 1.0.0**. Ver o ficheiro
[LICENSE.md](LICENSE.md) para mais detalhes. Software de terceiros (bibliotecas
Arduino/Adafruit/TinyGPSPlus/ArduinoJson) mantém as suas próprias licenças.
