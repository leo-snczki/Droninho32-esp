# Droninho32 — Firmware (ESP32-S3)

Firmware unificado e modular do drone **Droninho32**, escrito em C++ (Arduino core
para ESP32). Cria um **Access Point WiFi** e expõe uma **API REST/JSON** para
controlo dos 4 motores, leitura de sensores (IMU + GPS) e telemetria, conforme o
contrato em [`docs/ARCHITECTURE.md`](../docs/ARCHITECTURE.md) (secções 2, 3, 7, 8).

> **Aviso de segurança (ler primeiro):** este é um drone experimental. Os motores
> podem causar ferimentos. **Testa sempre SEM HÉLICES** até teres a certeza de que
> tudo funciona. Mantém o limite `THROTTLE_MAX_TEST` baixo durante os testes.

---

## Estrutura

```
droninho32-esp/
├─ Droninho32/                 # Sketch Arduino UNIFICADO — abrir ESTA pasta no Arduino IDE
│  ├─ Droninho32.ino           # setup()/loop(): orquestra os módulos
│  ├─ config.h                 # pinos, PWM, WiFi, sensores, limites de segurança
│  ├─ motors.h / motors.cpp    # ESC via ledc (50 Hz/14 bit), arm/disarm, mistura quad-X, rampa
│  ├─ imu.h / imu.cpp          # MPU6050 (Adafruit) + filtro complementar (roll/pitch/yaw)
│  ├─ gps.h / gps.cpp          # NEO-6M via TinyGPSPlus (lat/lon/alt/sats/speed/course/epoch)
│  ├─ telemetry.h / .cpp       # monta o JSON TelemetryPoint (ArduinoJson v7)
│  ├─ comms.h / comms.cpp      # SoftAP + WebServer + API /api/* + heartbeat/failsafe + estados
│  └─ web_ui.h                 # HTML (PROGMEM) da página de controlo manual
├─ Testes/                     # protótipos anteriores (Teste1/2/3) — mantidos como referência
├─ specs ESP32S3.txt           # info da placa (chip, flash, MAC)
└─ README.md                   # este ficheiro
```

A maioria dos sketches Arduino tem **um** `.ino`. Aqui o `.ino` inclui vários
`.h/.cpp` na **mesma pasta** — o Arduino IDE compila-os automaticamente (cada
`.cpp` torna-se uma *translation unit*; os `namespace` evitam colisões de nomes).

---

## Bibliotecas necessárias

Instalar pelo **Library Manager** do Arduino IDE (*Sketch → Include Library →
Manage Libraries…*) ou via `arduino-cli lib install`:

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

> **ArduinoJson v7:** este código usa `JsonDocument` (sem capacidade fixa) e a API
> v7 (`doc["x"].to<JsonArray>()`, `.is<int>()`, `value | default`). Não funciona com a v6.

---

## Compilar e carregar (Arduino CLI)

A placa é **ESP32-S3** (FQBN `esp32:esp32:esp32s3`). A porta vista nas specs é `COM8`.

```bash
# Compilar
arduino-cli compile --fqbn esp32:esp32:esp32s3 Droninho32

# Carregar (ajusta a porta COM)
arduino-cli upload  --fqbn esp32:esp32:esp32s3 -p COM8 Droninho32

# Monitor série
arduino-cli monitor -p COM8 -b 115200
```

**Em alternativa (Arduino IDE):** abrir a pasta `Droninho32/`, selecionar a placa
*ESP32S3 Dev Module*, *Upload Speed* 115200, e carregar.

> Esta máquina **não tem o arduino-cli instalado**, por isso o firmware **não foi
> compilado aqui**. Foi escrito com cuidado (sintaxe C++/Arduino, includes e tipos
> coerentes com o core ESP32 v3.x e ArduinoJson v7), mas deve ser compilado no
> Arduino IDE / arduino-cli antes do primeiro upload.

---

## Pinout (ver e ajustar em `config.h`)

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

> A ordem **FL, FR, RL, RR** é a convenção fixa do contrato (secção 2). Se os teus
> ESC estiverem ligados a outros pinos, muda só o array `MOTOR_PINS` em `config.h`.

### Esquema de ligações

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

---

## Sequência de calibração dos ESC (sem hélices!)

Os ESC precisam de aprender os pulsos mínimo/máximo. Faz isto **uma vez** por ESC
(ou todos juntos), **sem hélices montadas** e com a bateria ligada:

1. **Não** ligues a bateria ainda.
2. Carrega um sketch que envie o **pulso máximo** (2000 µs) — podes usar
   temporariamente o `THROTTLE_MAX_TEST` no máximo num teste de bancada, ou um
   sketch de calibração dedicado.
3. Liga a bateria. O ESC emite os *beeps* de "throttle máximo detetado".
4. Baixa o sinal para o **pulso mínimo** (1000 µs). O ESC confirma com *beeps*.
5. O ESC fica calibrado. Desliga e volta ao firmware normal.

> O firmware normal já arranca no **mínimo (1000 µs)**, por isso, depois de
> calibrado, basta ligar — os motores ficam parados até `arm` + `throttle`.

---

## Checklist de SEGURANÇA (obrigatório antes de cada teste)

- [ ] **Hélices removidas** durante todos os testes de software.
- [ ] `THROTTLE_MAX_TEST` está a **100%** (limite removido a pedido) → **só testar SEM hélices** e subir o acelerador devagar, à mão.
- [ ] Alimentação dos motores pela **bateria**, ESP pelo USB só para debug.
- [ ] Confirmar no boot: estado `idle`, motores no mínimo, **não giram**.
- [ ] Testar `arm` → motores continuam parados (throttle 0).
- [ ] Subir `throttle` devagar e confirmar a **rampa** (sem saltos).
- [ ] Testar `stop` — corte imediato.
- [ ] Testar **failsafe**: parar de enviar heartbeat → desarma sozinho em ~1,5 s.
- [ ] Só montar hélices depois de tudo isto validado, em ambiente seguro.

---

## API do drone (resumo — ver contrato secção 3)

Servidor HTTP em **`http://192.168.4.1`** (porta 80). Respostas JSON, CORS `*`.

| Método | Rota | Descrição |
|--------|------|-----------|
| `GET` | `/` | UI web de controlo manual (HTML) |
| `GET` | `/api/status` | `{ name, firmware, uptime_ms, armed, status, ip }` |
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
{ "cmd": "heartbeat" }                           // mantém o failsafe satisfeito (~2 Hz)
```

**Exemplos `curl`** (depois de ligar o PC ao WiFi `Droninho32`):

```bash
curl http://192.168.4.1/api/status
curl http://192.168.4.1/api/telemetry
curl -X POST http://192.168.4.1/api/command -H "Content-Type: application/json" -d '{"cmd":"arm"}'
curl -X POST http://192.168.4.1/api/command -H "Content-Type: application/json" -d '{"cmd":"throttle","value":20}'
curl -X POST http://192.168.4.1/api/command -H "Content-Type: application/json" -d '{"cmd":"stop"}'
```

---

## Máquina de estados

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

---

## Comportamento sem GPS / sem sensor de bateria (MVP)

O firmware funciona mesmo **sem GPS e sem medição de bateria** ligados:

- **IMU (MPU6050):** se não responder, a telemetria reporta atitude/aceleração a `0`.
  Recomenda-se tê-la ligada — é o sensor central do MVP.
- **GPS (NEO-6M):** sem fix, `gps.fix=false` e `lat/lon=0`. O drone controla-se na mesma.
- **Bateria:** `BATTERY_ENABLED 0` por defeito → `voltage=0`, `percent=0`.

Assim, **controlo manual + telemetria de atitude** funcionam de imediato.

---

## Como estender

- **PID / estabilização em malha fechada:** consumir `imu::get()` (roll/pitch/yaw)
  e corrigir os alvos em `motors::setRC` antes da mistura. (Futuro, contrato §9.)
- **WebSocket de telemetria** (`/ws`, 5–10 Hz): adicionar em `comms.cpp`; o MVP usa
  *polling* a `/api/telemetry`.
- **ESP32-CAM (vídeo):** **implementada** — sketch separado em `Droninho32_CAM/`. Liga-se ao AP `Droninho32` como estação (IP fixo `192.168.4.2`) e transmite MJPEG em `http://192.168.4.2/stream`. Compilar/gravar com a board **AI Thinker ESP32-CAM** (`esp32:esp32:esp32cam`); na ESP32-CAM, liga **IO0→GND** para entrar em modo de gravação. O firmware do S3 anuncia o `camera_url` em `/api/status`.
- **Medição de bateria:** ligar um divisor resistivo ao ADC e pôr `BATTERY_ENABLED 1`.
- **Mudar pinos/limites:** tudo em `config.h` — nenhum módulo tem "números mágicos".

---

## License

Este projeto está licenciado sob a **PolyForm Noncommercial License 1.0.0**.
Ver o ficheiro [LICENSE.md](LICENSE.md) para mais detalhes. Software de terceiros
(bibliotecas Arduino/Adafruit/TinyGPSPlus/ArduinoJson) mantém as suas próprias licenças.
