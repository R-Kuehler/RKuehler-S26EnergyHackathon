<?php
// index.php — Solar Energy Debt Tracker Live Dashboard
// Maroon & Gold theme | Auto-refreshes every 2 seconds via JS fetch

$data_file = __DIR__ . '/solar_data.json';
$latest = [];
if (file_exists($data_file)) {
    $latest = json_decode(file_get_contents($data_file), true) ?? [];
}

// State display helpers
function stateLabel($s) {
    return match($s) {
        'SURPLUS_PLUS' => '⚡ ++SURPLUS++',
        'SURPLUS'      => '✔ SURPLUS',
        'DEFICIT'      => '⚠ DEFICIT',
        default        => '— UNKNOWN'
    };
}
function stateClass($s) {
    return match($s) {
        'SURPLUS_PLUS' => 'state-surplus-plus',
        'SURPLUS'      => 'state-surplus',
        'DEFICIT'      => 'state-deficit',
        default        => 'state-unknown'
    };
}
?>
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Solar Energy Tracker — Dashboard</title>
<style>
  :root {
    --maroon:  #800000;
    --maroon2: #5c0000;
    --gold:    #FFD700;
    --gold2:   #b8960c;
    --bg:      #1a0000;
    --surface: #2b0000;
    --surface2:#3d0000;
    --text:    #f5e6c8;
    --muted:   #a08060;
  }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    background: var(--bg);
    color: var(--text);
    font-family: 'Segoe UI', sans-serif;
    min-height: 100vh;
  }
  header {
    background: linear-gradient(135deg, var(--maroon2), var(--maroon));
    border-bottom: 3px solid var(--gold);
    padding: 18px 32px;
    display: flex;
    align-items: center;
    justify-content: space-between;
  }
  header h1 {
    color: var(--gold);
    font-size: 1.6rem;
    letter-spacing: 2px;
    text-transform: uppercase;
  }
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
    transition: border-color 0.2s;
  }
  nav a:hover, nav a.active { border-bottom-color: var(--gold); }

  main { padding: 28px 32px; }

  .last-update {
    color: var(--muted);
    font-size: 0.8rem;
    margin-bottom: 20px;
  }

  /* ── Stat Cards ── */
  .cards {
    display: grid;
    grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
    gap: 18px;
    margin-bottom: 32px;
  }
  .card {
    background: var(--surface);
    border: 1px solid var(--maroon);
    border-top: 3px solid var(--gold);
    border-radius: 8px;
    padding: 18px 20px;
    transition: transform 0.15s;
  }
  .card:hover { transform: translateY(-2px); }
  .card .label {
    color: var(--muted);
    font-size: 0.75rem;
    letter-spacing: 1.5px;
    text-transform: uppercase;
    margin-bottom: 8px;
  }
  .card .value {
    color: var(--gold);
    font-size: 2rem;
    font-weight: 700;
    line-height: 1;
  }
  .card .unit {
    color: var(--muted);
    font-size: 0.8rem;
    margin-top: 4px;
  }

  /* ── State badge ── */
  .state-card {
    background: var(--surface);
    border: 1px solid var(--maroon);
    border-radius: 8px;
    padding: 18px 20px;
    display: flex;
    align-items: center;
    gap: 16px;
  }
  .state-badge {
    font-size: 1.2rem;
    font-weight: bold;
    padding: 8px 20px;
    border-radius: 6px;
  }
  .state-surplus-plus { background: #1a4700; color: #7fff00; border: 1px solid #7fff00; }
  .state-surplus      { background: #1a3500; color: #90ee90; border: 1px solid #90ee90; }
  .state-deficit      { background: #4a0000; color: #ff6b6b; border: 1px solid #ff6b6b; }
  .state-unknown      { background: #2a2a2a; color: #aaa;    border: 1px solid #555; }

  /* ── Servo Gauge ── */
  .servo-section {
    background: var(--surface);
    border: 1px solid var(--maroon);
    border-top: 3px solid var(--gold);
    border-radius: 8px;
    padding: 20px 24px;
    margin-bottom: 32px;
  }
  .servo-section h2 {
    color: var(--gold);
    font-size: 0.85rem;
    letter-spacing: 2px;
    text-transform: uppercase;
    margin-bottom: 16px;
  }
  .servo-arc-wrap {
    display: flex;
    align-items: center;
    gap: 32px;
    flex-wrap: wrap;
  }
  .servo-arc {
    position: relative;
    width: 220px;
    height: 120px;
  }
  .servo-arc svg { width: 100%; height: 100%; }
  .servo-meta { color: var(--text); font-size: 0.9rem; line-height: 2; }
  .servo-meta span { color: var(--gold); font-weight: bold; }

  /* ── Harvest bar ── */
  .harvest-bar-wrap {
    background: var(--surface);
    border: 1px solid var(--maroon);
    border-top: 3px solid var(--gold);
    border-radius: 8px;
    padding: 20px 24px;
    margin-bottom: 32px;
  }
  .harvest-bar-wrap h2 {
    color: var(--gold);
    font-size: 0.85rem;
    letter-spacing: 2px;
    text-transform: uppercase;
    margin-bottom: 12px;
  }
  .bar-track {
    background: var(--surface2);
    border-radius: 6px;
    height: 28px;
    width: 100%;
    overflow: hidden;
    border: 1px solid var(--maroon);
  }
  .bar-fill {
    height: 100%;
    background: linear-gradient(90deg, var(--gold2), var(--gold));
    border-radius: 6px;
    transition: width 0.8s ease;
    display: flex;
    align-items: center;
    padding-left: 10px;
    font-size: 0.8rem;
    font-weight: bold;
    color: var(--maroon2);
    min-width: 40px;
  }
  .bar-labels {
    display: flex;
    justify-content: space-between;
    color: var(--muted);
    font-size: 0.75rem;
    margin-top: 4px;
  }

  footer {
    text-align: center;
    padding: 16px;
    color: var(--muted);
    font-size: 0.75rem;
    border-top: 1px solid var(--maroon);
    margin-top: 16px;
  }
</style>
</head>
<body>

<header>
  <div>
    <h1>☀ Solar Energy Tracker</h1>
    <div class="subtitle">ESP32 DevKit v1 · LDR + SSD1306 + Servo</div>
  </div>
  <span class="live-badge" id="live-badge">● LIVE</span>
</header>

<nav>
  <a href="index.php" class="active">Dashboard</a>
  <a href="charts.php">Charts</a>
</nav>

<main>
  <div class="last-update" id="last-update">
    Last update: <?= htmlspecialchars($latest['ts'] ?? 'No data yet') ?>
  </div>

  <!-- Stat Cards -->
  <div class="cards" id="cards">
    <div class="card">
      <div class="label">LDR Raw Value</div>
      <div class="value" id="v-ldr"><?= $latest['ldr'] ?? '—' ?></div>
      <div class="unit">ADC counts (0–4095)</div>
    </div>
    <div class="card">
      <div class="label">Harvest Power</div>
      <div class="value" id="v-harvest"><?= $latest['p_harvest'] ?? '—' ?></div>
      <div class="unit">mW (0–500 mW max)</div>
    </div>
    <div class="card">
      <div class="label">Energy Debt</div>
      <div class="value" id="v-edebt"><?= isset($latest['e_debt']) ? number_format($latest['e_debt'],2) : '—' ?></div>
      <div class="unit">mWh (+ = surplus, − = deficit)</div>
    </div>
    <div class="card">
      <div class="label">Battery Voltage</div>
      <div class="value" id="v-vbatt"><?= isset($latest['v_batt']) ? number_format($latest['v_batt'],2) : '—' ?></div>
      <div class="unit">V</div>
    </div>
    <div class="card">
      <div class="label">Battery SOC</div>
      <div class="value" id="v-soc"><?= isset($latest['soc']) ? number_format($latest['soc'],1) : '—' ?></div>
      <div class="unit">%</div>
    </div>
  </div>

  <!-- Operating State -->
  <div class="state-card" style="margin-bottom:24px;">
    <div class="label" style="min-width:120px; color:var(--muted); font-size:0.75rem; letter-spacing:1.5px; text-transform:uppercase;">
      Operating State
    </div>
    <div class="state-badge <?= stateClass($latest['state'] ?? '') ?>" id="v-state">
      <?= stateLabel($latest['state'] ?? '') ?>
    </div>
    <div style="color:var(--muted); font-size:0.8rem; margin-left:16px;">
      Load assumed: <strong style="color:var(--gold)">150 mW</strong>
    </div>
  </div>

  <!-- Harvest Power Bar -->
  <div class="harvest-bar-wrap">
    <h2>Harvest Power Meter</h2>
    <?php
      $pct = isset($latest['p_harvest']) ? min(100, ($latest['p_harvest'] / 500) * 100) : 0;
    ?>
    <div class="bar-track">
      <div class="bar-fill" id="harvest-bar" style="width:<?= $pct ?>%">
        <?= isset($latest['p_harvest']) ? $latest['p_harvest'].' mW' : '' ?>
      </div>
    </div>
    <div class="bar-labels"><span>0 mW</span><span>250 mW</span><span>500 mW</span></div>
  </div>

  <!-- Servo Arc Gauge -->
  <div class="servo-section">
    <h2>Solar Tracker Servo</h2>
    <div class="servo-arc-wrap">
      <div class="servo-arc">
        <?php
          $pos = $latest['servo_pos'] ?? 90;
          // Map 63–117° to SVG arc: center at (110,110), radius 90
          // 63° servo = -90° arc position (left), 117° = +90° arc
          $arc_pct = ($pos - 63) / 54; // 0.0 to 1.0
          $arc_angle = -90 + ($arc_pct * 180); // -90 to +90 degrees
          $rad = deg2rad($arc_angle);
          $cx = 110; $cy = 110; $r = 85;
          $nx = $cx + $r * cos($rad);
          $ny = $cy + $r * sin($rad);
        ?>
        <svg viewBox="0 0 220 120" xmlns="http://www.w3.org/2000/svg">
          <!-- Arc track -->
          <path d="M 25,110 A 85,85 0 0,1 195,110"
                fill="none" stroke="#3d0000" stroke-width="10" stroke-linecap="round"/>
          <!-- Arc fill (gold) -->
          <path d="M 25,110 A 85,85 0 0,1 <?= round($nx,1) ?>,<?= round($ny,1) ?>"
                fill="none" stroke="#FFD700" stroke-width="10" stroke-linecap="round"
                id="arc-fill"/>
          <!-- Needle dot -->
          ircle cx="<?= round($nx,1) ?>" cy="<?= round($ny,1) ?>" r="8"
                  fill="#FFD700" id="servo-dot"/>
          <!-- Labels -->
          <text x="18" y="118" fill="#a08060" font-size="10">63°</text>
          <text x="189" y="118" fill="#a08060" font-size="10">117°</text>
          <text x="100" y="108" fill="#FFD700" font-size="13" font-weight="bold"
                text-anchor="middle" id="servo-deg-svg"><?= $pos ?>°</text>
        </svg>
      </div>
      <div class="servo-meta">
        <div>Position: <span id="servo-pos-txt"><?= $pos ?>°</span></div>
        <div>Direction: <span id="servo-dir-txt"><?= (($latest['servo_dir'] ?? 1) > 0) ? '→ Sweeping Right' : '← Sweeping Left' ?></span></div>
        <div>Arc range: <span>63° – 117°</span></div>
        <div>Step size: <span>3° / step</span></div>
      </div>
    </div>
  </div>
</main>

<footer>
  Solar Energy Debt Tracker · ESP32 DevKit v1 · XAMPP Local Server
</footer>

<script>
// Auto-refresh live data every 2 seconds via fetch
async function refreshData() {
  try {
    const resp = await fetch('solar_data.json?nc=' + Date.now());
    if (!resp.ok) return;
    const d = await resp.json();

    document.getElementById('last-update').textContent = 'Last update: ' + (d.ts || 'unknown');
    document.getElementById('v-ldr').textContent      = d.ldr ?? '—';
    document.getElementById('v-harvest').textContent  = d.p_harvest ?? '—';
    document.getElementById('v-edebt').textContent    = d.e_debt != null ? parseFloat(d.e_debt).toFixed(2) : '—';
    document.getElementById('v-vbatt').textContent    = d.v_batt != null ? parseFloat(d.v_batt).toFixed(2) : '—';
    document.getElementById('v-soc').textContent      = d.soc != null ? parseFloat(d.soc).toFixed(1) : '—';

    // State badge
    const stateEl = document.getElementById('v-state');
    const stateMap = {
      'SURPLUS_PLUS': ['⚡ ++SURPLUS++', 'state-surplus-plus'],
      'SURPLUS':      ['✔ SURPLUS',      'state-surplus'],
      'DEFICIT':      ['⚠ DEFICIT',      'state-deficit']
    };
    const [label, cls] = stateMap[d.state] || ['— UNKNOWN', 'state-unknown'];
    stateEl.textContent = label;
    stateEl.className = 'state-badge ' + cls;

    // Harvest bar
    const pct = Math.min(100, ((d.p_harvest || 0) / 500) * 100);
    const bar = document.getElementById('harvest-bar');
    bar.style.width = pct + '%';
    bar.textContent = (d.p_harvest || 0) + ' mW';

    // Servo arc — re-calculate SVG points
    const pos = d.servo_pos ?? 90;
    const arcPct = (pos - 63) / 54;
    const arcAngle = -90 + (arcPct * 180);
    const rad = arcAngle * Math.PI / 180;
    const cx = 110, cy = 110, r = 85;
    const nx = cx + r * Math.cos(rad);
    const ny = cy + r * Math.sin(rad);

    document.getElementById('servo-dot').setAttribute('cx', nx.toFixed(1));
    document.getElementById('servo-dot').setAttribute('cy', ny.toFixed(1));
    document.getElementById('arc-fill').setAttribute('d',
      `M 25,110 A 85,85 0 0,1 ${nx.toFixed(1)},${ny.toFixed(1)}`);
    document.getElementById('servo-deg-svg').textContent = pos + '°';
    document.getElementById('servo-pos-txt').textContent = pos + '°';
    document.getElementById('servo-dir-txt').textContent =
      (d.servo_dir > 0) ? '→ Sweeping Right' : '← Sweeping Left';

  } catch(e) { console.warn('Refresh error:', e); }
}

setInterval(refreshData, 2000);
</script>
</body>
</html>