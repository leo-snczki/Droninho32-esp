// ============================================================================
//  Droninho32 — web_ui.h
//  Pagina HTML de controlo manual (servida em GET /). Guardada em PROGMEM
//  para nao ocupar RAM. Evolui a UI do Teste3: arma/desarma, slider de
//  throttle, paragem de emergencia e telemetria ao vivo.
//
//  A pagina fala com a propria API JSON do drone:
//    POST /api/command   {cmd:...}
//    GET  /api/telemetry -> TelemetryPoint
//
//  AVISO: e uma UI de teste/depuracao. O controlo "a serio" e a telemetria
//  ao vivo sao feitos pela app Android. Testar SEMPRE sem helices primeiro.
// ============================================================================
#ifndef DRONINHO32_WEB_UI_H
#define DRONINHO32_WEB_UI_H

#include <Arduino.h>

// String literal em PROGMEM (flash). R"=====( ... )=====" = raw string.
static const char DRONINHO32_INDEX_HTML[] PROGMEM = R"=====(<!DOCTYPE html>
<html lang="pt">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Droninho32 — Controlo</title>
  <style>
    :root { --bg:#0f172a; --card:#1e293b; --txt:#e2e8f0; --mut:#94a3b8;
            --ok:#22c55e; --warn:#f59e0b; --bad:#ef4444; --acc:#38bdf8; }
    * { box-sizing: border-box; }
    body { font-family: system-ui, Arial, sans-serif; margin:0; padding:16px;
           background:var(--bg); color:var(--txt); }
    h1 { font-size:1.3rem; margin:0 0 4px; }
    .sub { color:var(--mut); font-size:.85rem; margin-bottom:16px; }
    .card { background:var(--card); border-radius:12px; padding:16px;
            margin-bottom:14px; box-shadow:0 4px 10px rgba(0,0,0,.3); }
    .row { display:flex; gap:10px; flex-wrap:wrap; }
    button { flex:1 1 120px; padding:14px; font-size:1rem; font-weight:700;
             border:none; border-radius:10px; color:#fff; cursor:pointer; }
    .b-arm { background:var(--ok); } .b-dis { background:var(--warn); }
    .b-stop{ background:var(--bad); font-size:1.2rem; }
    .b-acc { background:var(--acc); color:#08233a; }
    button:active { transform:scale(.98); }
    label { display:block; margin:10px 0 4px; color:var(--mut); font-size:.85rem; }
    input[type=range]{ width:100%; }
    .thr { font-size:2rem; font-weight:800; text-align:center; }
    .grid { display:grid; grid-template-columns:1fr 1fr; gap:8px; }
    .kv { background:#0b1220; border-radius:8px; padding:8px 10px; font-size:.85rem; }
    .kv b { color:var(--acc); }
    .badge { display:inline-block; padding:3px 10px; border-radius:999px;
             font-size:.8rem; font-weight:700; }
    .s-idle{background:#334155;} .s-armed{background:#a16207;}
    .s-flying{background:#15803d;} .s-failsafe{background:#b91c1c;}
    .s-error{background:#7f1d1d;}
    .mono{ font-family: ui-monospace, monospace; }
  </style>
</head>
<body>
  <h1>Droninho32 &mdash; Controlo</h1>
  <div class="sub">UI de teste. <b>Testar sempre SEM helices primeiro.</b></div>

  <div class="card">
    <div class="row">
      <span>Estado: <span id="st" class="badge s-idle">--</span></span>
      <span style="margin-left:auto" class="mono" id="conn">a ligar...</span>
    </div>
  </div>

  <div class="card">
    <div class="row">
      <button class="b-arm" onclick="cmd({cmd:'arm'})">ARMAR</button>
      <button class="b-dis" onclick="cmd({cmd:'disarm'})">DESARMAR</button>
    </div>
    <div class="row" style="margin-top:10px">
      <button class="b-stop" onclick="emergency()">&#9632; STOP DE EMERGENCIA</button>
    </div>
  </div>

  <div class="card">
    <label for="thr">Acelerador geral (0&ndash;100%)</label>
    <div class="thr"><span id="thrVal">0</span>%</div>
    <input type="range" id="thr" min="0" max="100" value="0"
           oninput="document.getElementById('thrVal').innerText=this.value"
           onchange="cmd({cmd:'throttle', value:parseInt(this.value)})">
    <div class="sub">Limitado por seguranca no firmware (THROTTLE_MAX_TEST).</div>
  </div>

  <div class="card">
    <label>Telemetria ao vivo</label>
    <div class="grid">
      <div class="kv">Armado: <b id="tArm">--</b></div>
      <div class="kv">Uptime: <b id="tUp">--</b> s</div>
      <div class="kv">Roll: <b id="tRoll">--</b>&deg;</div>
      <div class="kv">Pitch: <b id="tPitch">--</b>&deg;</div>
      <div class="kv">Yaw: <b id="tYaw">--</b>&deg;</div>
      <div class="kv">Throttle: <b id="tThr">--</b>%</div>
      <div class="kv">Motores: <b id="tMot" class="mono">--</b></div>
      <div class="kv">GPS fix: <b id="tFix">--</b> (<span id="tSat">0</span> sat)</div>
      <div class="kv">Lat: <b id="tLat" class="mono">--</b></div>
      <div class="kv">Lon: <b id="tLon" class="mono">--</b></div>
      <div class="kv">Bateria: <b id="tBat">--</b>%</div>
      <div class="kv">RSSI: <b id="tRssi">--</b> dBm</div>
    </div>
  </div>

  <script>
    // Envia um comando POST /api/command e mantem o heartbeat satisfeito.
    async function cmd(obj){
      try{
        const r = await fetch('/api/command', {
          method:'POST', headers:{'Content-Type':'application/json'},
          body: JSON.stringify(obj)
        });
        return await r.json();
      }catch(e){ console.error(e); }
    }
    // STOP: comando de paragem imediata + repoe o slider a 0.
    function emergency(){
      cmd({cmd:'stop'});
      const s = document.getElementById('thr');
      s.value = 0; document.getElementById('thrVal').innerText = '0';
    }
    // Heartbeat a ~2 Hz (mantem o failsafe do firmware satisfeito).
    setInterval(()=>cmd({cmd:'heartbeat'}), 500);

    // Atualiza a telemetria a ~3 Hz.
    function fmt(n,d){ return (typeof n==='number') ? n.toFixed(d) : n; }
    async function poll(){
      try{
        const r = await fetch('/api/telemetry');
        const t = await r.json();
        document.getElementById('conn').innerText = 'ligado';
        const st = document.getElementById('st');
        st.innerText = t.status; st.className = 'badge s-' + t.status;
        document.getElementById('tArm').innerText  = t.armed ? 'SIM' : 'nao';
        document.getElementById('tUp').innerText   = Math.floor(t.uptime_ms/1000);
        document.getElementById('tRoll').innerText = fmt(t.attitude.roll,1);
        document.getElementById('tPitch').innerText= fmt(t.attitude.pitch,1);
        document.getElementById('tYaw').innerText  = fmt(t.attitude.yaw,1);
        document.getElementById('tThr').innerText  = t.throttle;
        document.getElementById('tMot').innerText  = '['+t.motors.join(', ')+']';
        document.getElementById('tFix').innerText  = t.gps.fix ? 'SIM' : 'nao';
        document.getElementById('tSat').innerText  = t.gps.sats;
        document.getElementById('tLat').innerText  = fmt(t.gps.lat,6);
        document.getElementById('tLon').innerText  = fmt(t.gps.lon,6);
        document.getElementById('tBat').innerText  = t.battery.percent;
        document.getElementById('tRssi').innerText = t.rssi;
      }catch(e){
        document.getElementById('conn').innerText = 'sem ligacao';
      }
    }
    setInterval(poll, 333);
    poll();
  </script>
</body>
</html>)=====";

#endif // DRONINHO32_WEB_UI_H
