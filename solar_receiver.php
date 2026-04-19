<?php
// solar_receiver.php
// Accepts POST from ESP32, writes latest snapshot + rolling 500-pt log.
// Place in: htdocs/EnergyHackathonS2026/

header('Access-Control-Allow-Origin: *');
header('Access-Control-Allow-Methods: POST, GET, OPTIONS');
header('Access-Control-Allow-Headers: Content-Type');
header('Content-Type: application/json');

// Handle preflight OPTIONS request
if ($_SERVER['REQUEST_METHOD'] === 'OPTIONS') {
    http_response_code(200);
    exit;
}

$data_file  = __DIR__ . '/solar_data.json';
$log_file   = __DIR__ . '/solar_log.json';
$max_points = 500;

// Allow GET for browser-based testing: ?ldr=3000&p_harvest=350&...
$source = ($_SERVER['REQUEST_METHOD'] === 'POST') ? $_POST : $_GET;

if (empty($source)) {
    http_response_code(400);
    echo json_encode(['error' => 'No data received. Use POST or GET with parameters.']);
    exit;
}

$entry = [
    'ts'        => date('Y-m-d H:i:s'),
    'ldr'       => intval($source['ldr']       ?? 0),
    'p_harvest' => intval($source['p_harvest'] ?? 0),
    'e_debt'    => floatval($source['e_debt']  ?? 0.0),
    'v_batt'    => floatval($source['v_batt']  ?? 0.0),
    'soc'       => floatval($source['soc']     ?? 0.0),
    'servo_pos' => intval($source['servo_pos'] ?? 90),
    'servo_dir' => intval($source['servo_dir'] ?? 1),
    'state'     => preg_replace('/[^A-Z_]/', '', strtoupper($source['state'] ?? 'UNKNOWN')),
];

// Write latest snapshot
file_put_contents($data_file, json_encode($entry), LOCK_EX);

// Append to rolling log
$log = [];
if (file_exists($log_file)) {
    $raw = file_get_contents($log_file);
    $log = json_decode($raw, true) ?? [];
}
$log[] = $entry;
if (count($log) > $max_points) {
    $log = array_slice($log, -$max_points);
}
file_put_contents($log_file, json_encode($log), LOCK_EX);

echo json_encode(['status' => 'ok', 'ts' => $entry['ts']]);
?>