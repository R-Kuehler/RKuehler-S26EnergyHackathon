<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Solar Energy Tracker - Charts</title>
<script src="chart.umd.min.js"></script>
<style>
:root {
  --maroon:  #800000;
  --maroon2: #5c0000;
  --gold:    #FFD700;
  --bg:      #1a0000;
  --surface: #2b0000;
  --text:    #f5e6c8;
  --muted:   #a08060;
}
* { box-sizing: border-box; margin: 0; padding: 0; }
body { background: var(--bg); color: var(--text); font-family: 'Segoe UI', sans-serif; }
header {
  background: linear-gradient(135deg, var(--maroon2), var(--maroon));
  border-bottom: 3px solid var(--gold);
  padding: 18px 32px;
  display: flex;
  align-items: center;
  justify-content: space-between;
}
header h1 { color: var(--gold); font-size: 1.6rem; letter-spacing: 2px; text-transform: uppercase; }
header .subtitle { color: var(--muted); font-size: 0.85rem; margin-top: 4px; }
.live-badge {
  background: var(--gold);
  color: var(--maroon2);
  font-weight: bold;
  font-size: 0.75rem;
  padding: 4px 12px;
  border-radius: 20px;
  animation: pulse 1.5s infinite;
}
@keyframes pulse { 0%,100%{opacity:1} 50%{opacity:0.5} }
nav {
  background: var(--surface);
  border-bottom: 1px solid var(--maroon);
  padding: 10px 32px;
  display: flex;
  gap: 24px;
}
nav a {
  color: var(--gold);
  text-decoration: none;
  font-size: 0.9rem;
  letter-spacing: 1px;
  padding: 4px 0;
  border-bottom: 2px solid transparent;
}
nav a.active { border-bottom-color: var(--gold); }
main { padding: 28px 32px; }
.status-bar {
  display: flex;
  flex-wrap: wrap;
  gap: 16px;
  margin-bottom: 20px;
  padding: 10px 16px;
  background: var(--surface);
  border: 1px solid var(--maroon);
  border-radius: 6px;
  font-size: 0.82rem;
}
.status-bar .lbl { color: var(--muted); }
.status-bar .val { color: var(--gold); font-weight: bold; margin-left: 4px; }
.chart-block {
  background: var(--surface);
  border: 1px solid var(--maroon);
  border-top: 3px solid var(--gold);
  border-radius: 8px;
  padding: 20px 24px;
  margin-bottom: 28px;
}
.chart-block h2 {
  color: var(--gold);
  font-size: 0.85rem;
  letter-spacing: 2px;
  text-transform: uppercase;
  margin-bottom: 16px;
}
.chart-wrap { position: relative; height: 240px; width: 100%; }
footer {
  text-align: center;
  padding: 16px;
  color: var(--muted);
  font-size: 0.75rem;
  border-top: 1px solid var(--maroon);
  margin-top: 8px;
}
</style>
</head>
<body>

<header>
  <div>
    <h1>Solar Energy Tracker</h1>
    <div class="subtitle">ESP32 DevKit v1 - LDR + SSD1306 + Servo</div>
  </div>
  <span class="live-badge">LIVE</span>
</header>

<nav>
  <a href="index.php">Dashboard</a>
  <a href="charts.php" class="active">Charts</a>
</nav>

<main>
  <div class="status-bar">
    <span><span class="lbl">LDR:</span><span class="val" id="sb-ldr">--</span></span>
    <span><span class="lbl">Harvest:</span><span class="val" id="sb-harvest">--</span> mW</span>
    <span><span class="lbl">E.Debt:</span><span class="val" id="sb-edebt">--</span> mWh</span>
    <span><span class="lbl">SOC:</span><span class="val" id="sb-soc">--</span> %</span>
    <span><span class="lbl">State:</span><span class="val" id="sb-state">--</span></span>
  </div>

  <div class="chart-block">
    <h2>Energy Debt / Surplus Over Time (mWh)</h2>
    <div class="chart-wrap"><canvas id="c1"></canvas></div>
  </div>

  <div class="chart-block">
    <h2>Solar Harvest Power Over Time (mW)</h2>
    <div class="chart-wrap"><canvas id="c2"></canvas></div>
  </div>

  <div class="chart-block">
    <h2>Battery State of Charge Over Time (%)</h2>
    <div class="chart-wrap"><canvas id="c3"></canvas></div>
  </div>
</main>

<footer>Solar Energy Debt Tracker - ESP32 DevKit v1 - XAMPP Local Server</footer>

<script>
var MAX_PTS = 100, TICK = 1000, MAX_HW = 500, PLOAD = 150;
var ldr = 2500, drift = 0, eDebt = 0, soc = 65, sPos = 90, sDir = 1;
var LB = [], ED = [], HV = [], SC = [], BL = [];

var tip = { backgroundColor:'#2b0000', titleColor:'#FFD700', bodyColor:'#f5e6c8', borderColor:'#800000', borderWidth:1 };
var xAx = { ticks:{ color:'#a08060', maxRotation:0, autoSkip:true, maxTicksLimit:8 }, grid:{ color:'#3d0000' } };
var yAx = { ticks:{ color:'#a08060' }, grid:{ color:'#3d0000' } };

var C1 = new Chart(document.getElementById('c1').getContext('2d'), {
  type:'line',
  data:{ labels:LB, datasets:[{ label:'Energy Debt (mWh)', data:ED, borderColor:'#FFD700', backgroundColor:'rgba(255,215,0,0.08)', borderWidth:2, pointRadius:0, fill:true, tension:0.3 }] },
  options:{ responsive:true, maintainAspectRatio:false, animation:{duration:0}, plugins:{ legend:{labels:{color:'#f5e6c8'}}, tooltip:tip }, scales:{ x:xAx, y:yAx } }
});

var C2 = new Chart(document.getElementById('c2').getContext('2d'), {
  type:'line',
  data:{ labels:LB, datasets:[
    { label:'Harvest Power (mW)', data:HV, borderColor:'#ffaa00', backgroundColor:'rgba(255,170,0,0.10)', borderWidth:2, pointRadius:0, fill:true, tension:0.3 },
    { label:'Load (150 mW)', data:BL, borderColor:'#ff6b6b', borderWidth:1, borderDash:[5,5], pointRadius:0, fill:false }
  ]},
  options:{ responsive:true, maintainAspectRatio:false, animation:{duration:0}, plugins:{ legend:{labels:{color:'#f5e6c8'}}, tooltip:tip }, scales:{ x:xAx, y:Object.assign({},yAx,{min:0,max:520}) } }
});

var C3 = new Chart(document.getElementById('c3').getContext('2d'), {
  type:'line',
  data:{ labels:LB, datasets:[{ label:'Battery SOC (%)', data:SC, borderColor:'#90ee90', backgroundColor:'rgba(144,238,144,0.08)', borderWidth:2, pointRadius:0, fill:true, tension:0.3 }] },
  options:{ responsive:true, maintainAspectRatio:false, animation:{duration:0}, plugins:{ legend:{labels:{color:'#f5e6c8'}}, tooltip:tip }, scales:{ x:xAx, y:Object.assign({},yAx,{min:0,max:100}) } }
});

function tick() {
  drift += (Math.random() - 0.49) * 80;
  drift = Math.max(-600, Math.min(600, drift));
  ldr = Math.round(2500 + drift + Math.sin(Date.now() / 8000) * 700);
  ldr = Math.max(1300, Math.min(4095, ldr));

  var ph = Math.round(Math.max(0, Math.min(MAX_HW, (ldr-1300)/(4095-1300)*MAX_HW)));
  var dp = ph - PLOAD;
  eDebt += dp / 3600;
  soc   += (dp / MAX_HW) * 0.05;
  soc    = Math.max(0, Math.min(100, soc));

  var state = ldr >= 2800 ? '++SURPLUS++' : ldr <= 2150 ? 'DEFICIT' : 'SURPLUS';

  sPos += sDir * 3;
  if (sPos >= 117) { sPos = 117; sDir = -1; }
  if (sPos <= 63)  { sPos = 63;  sDir =  1; }

  var d = new Date();
  var ts = ('0'+d.getHours()).slice(-2)+':'+('0'+d.getMinutes()).slice(-2)+':'+('0'+d.getSeconds()).slice(-2);

  LB.push(ts); ED.push(+eDebt.toFixed(3)); HV.push(ph); SC.push(+soc.toFixed(1)); BL.push(PLOAD);
  if (LB.length > MAX_PTS) { LB.shift(); ED.shift(); HV.shift(); SC.shift(); BL.shift(); }

  C1.update(); C2.update(); C3.update();

  document.getElementById('sb-ldr').textContent     = ldr;
  document.getElementById('sb-harvest').textContent = ph;
  document.getElementById('sb-edebt').textContent   = eDebt.toFixed(2);
  document.getElementById('sb-soc').textContent     = soc.toFixed(1);
  document.getElementById('sb-state').textContent   = state;
}

setInterval(tick, TICK);
tick();
</script>
</body>
</html>