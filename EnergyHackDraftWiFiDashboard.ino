// ─────────────────────────────────────────────────────────────────
//  Solar Energy Debt Tracker — LDR + SSD1306 OLED + Servo Tracker
//  ESP32 DevKit v1  |  WiFi → PHP Dashboard Edition
//
//  Libraries required (Arduino IDE > Library Manager):
//    - Adafruit SSD1306
//    - Adafruit GFX Library
//    - ESP32Servo
//    - WiFi (built-in ESP32 core)
//    - HTTPClient (built-in ESP32 core)
//
//  Wiring:
//    LDR pin 1     ──► 3.3V
//    LDR pin 2     ──► GPIO34  +  10kΩ resistor to GND  (voltage divider)
//    BAT sensor    ──► GPIO35  (voltage divider: Vin→100kΩ→GPIO35→10kΩ→GND)
//    OLED VCC      ──► 3.3V
//    OLED GND      ──► GND
//    OLED SDA      ──► GPIO21
//    OLED SCL      ──► GPIO22
//    Servo signal  ──► GPIO13
//    Servo VCC     ──► 5V (use external supply if servo causes resets)
//    Servo GND     ──► GND (shared with ESP32)
//    LED           ──► GPIO26 → 330Ω → GND
// ─────────────────────────────────────────────────────────────────

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ── WiFi & Server ─────────────────────────────────────────────────
#define WIFI_SSID     "SolarTrack"
#define WIFI_PASSWORD "solartracker123"
const char* API_URL = "http://192.168.4.5/EnergyHackathonS2026/solar_receiver.php";

// ── OLED ──────────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define OLED_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ── LDR ───────────────────────────────────────────────────────────
int LDR = 34;
int input_val_LDR = 0;

// ── Voltage Sensor ────────────────────────────────────────────────
int BAT_PIN = 35;
float v_batt = 0.0;
float soc    = 0.0;

// ── Energy Debt ───────────────────────────────────────────────────
float e_debt = 0.0;
unsigned long last_time = 0;

// ── State thresholds ──────────────────────────────────────────────
#define SURPLUS_THRESHOLD  2800
#define DEFICIT_THRESHOLD  2150

// ── Simulated max harvest power (mW) ─────────────────────────────
#define MAX_HARVEST_MW 500

// ── Servo tracker ─────────────────────────────────────────────────
#define SERVO_PIN              13
#define SERVO_CENTER           90
#define SERVO_HALF_ARC         27
#define SERVO_MIN_DEG          (SERVO_CENTER - SERVO_HALF_ARC)  // 63°
#define SERVO_MAX_DEG          (SERVO_CENTER + SERVO_HALF_ARC)  // 117°
#define SERVO_STEP             3
#define LDR_NOISE_FLOOR        30
#define SETTLE_THRESHOLD       2800
#define FAST_STEP_MS           40
#define SLOW_STEP_MS           300

Servo tracker;
int  servo_pos   = SERVO_CENTER;
int  servo_dir   = 1;
int  prev_ldr    = 0;
unsigned long last_step_time = 0;

// ── WiFi helpers ──────────────────────────────────────────────────
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(250);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected. IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\nWiFi failed — continuing offline.");
  }
}

void postToDashboard(int ldr, int p_harvest, float e_debt_val,
                     float voltage, float soc_val,
                     int srv_pos, int srv_dir, const char* state) {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(API_URL);
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");

  String payload = "ldr="      + String(ldr)
                 + "&p_harvest=" + String(p_harvest)
                 + "&e_debt="    + String(e_debt_val, 3)
                 + "&v_batt="    + String(voltage, 2)
                 + "&soc="       + String(soc_val, 1)
                 + "&servo_pos=" + String(srv_pos)
                 + "&servo_dir=" + String(srv_dir)
                 + "&state="     + String(state);

  int httpCode = http.POST(payload);
  if (httpCode != HTTP_CODE_OK) {
    Serial.print("POST error: ");
    Serial.println(httpCode);
  }
  http.end();
}

// ── Chase step ────────────────────────────────────────────────────
void runChaseStep(int current_ldr) {
  unsigned long step_interval = (current_ldr >= SETTLE_THRESHOLD) ? SLOW_STEP_MS : FAST_STEP_MS;
  if (millis() - last_step_time < step_interval) return;
  last_step_time = millis();

  if (prev_ldr - current_ldr > LDR_NOISE_FLOOR) {
    servo_dir = -servo_dir;
  }

  int next_pos = servo_pos + (servo_dir * SERVO_STEP);

  if (next_pos > SERVO_MAX_DEG) {
    next_pos  = SERVO_MAX_DEG;
    servo_dir = -1;
  } else if (next_pos < SERVO_MIN_DEG) {
    next_pos  = SERVO_MIN_DEG;
    servo_dir = 1;
  }

  tracker.write(next_pos);
  servo_pos = next_pos;
  prev_ldr  = current_ldr;
}

// ── Setup ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);
  pinMode(26, OUTPUT);

  tracker.attach(SERVO_PIN, 500, 2400);
  tracker.write(SERVO_CENTER);
  delay(500);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 not found — check wiring & I2C address");
    while (true);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(20, 20);
  display.println("Energy Tracker");
  display.setCursor(30, 35);
  display.println("Starting...");
  display.display();

  connectWiFi();

  delay(1500);
  last_time      = millis();
  prev_ldr       = analogRead(LDR);
  last_step_time = millis();
}

// ── Loop ──────────────────────────────────────────────────────────
void loop() {

  // 1. Read sensors
  input_val_LDR = analogRead(LDR);
  v_batt = analogRead(BAT_PIN) * (3.3 / 4095.0) * 5.0;
  soc = map(v_batt * 100, 000, 900, 0, 100);
  soc = constrain(soc, 0, 100);

  // 2. Servo chase
  runChaseStep(input_val_LDR);

  // 3. Harvest power
  int p_harvest = map(input_val_LDR, 1300, 4095, 0, MAX_HARVEST_MW);
  p_harvest = constrain(p_harvest, 0, MAX_HARVEST_MW);

  // 4. Energy debt
  float p_load = 150.0;
  float delta_p = p_harvest - p_load;
  unsigned long now = millis();
  float dt_hours = (now - last_time) / 3600000.0;
  e_debt += delta_p * dt_hours;
  last_time = now;

  // 5. Determine state
  const char* state;
  if (input_val_LDR >= SURPLUS_THRESHOLD) {
    state = "SURPLUS_PLUS";   // maps to ++SURPLUS++ (no ++ in URL encoding)
  } else if (input_val_LDR <= DEFICIT_THRESHOLD) {
    state = "DEFICIT";
  } else {
    state = "SURPLUS";
  }

  // 6. Serial output
  Serial.print("LDR: ");        Serial.print(input_val_LDR);
  Serial.print("  P_harvest: "); Serial.print(p_harvest);
  Serial.print(" mW  Voltage: "); Serial.print(v_batt);
  Serial.print("  Servo: ");     Serial.print(servo_pos);
  Serial.print(servo_dir > 0 ? "> " : "< ");
  Serial.println(state);

  // 7. LED
  if (strcmp(state, "DEFICIT") == 0) {
    digitalWrite(26, LOW);
  } else {
    digitalWrite(26, HIGH);
  }

  // 8. OLED
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("== Energy Tracker ==");
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  display.setCursor(0, 13);
  display.print("LDR:");
  display.print(input_val_LDR);
  display.print(" Srv:");
  display.print(servo_pos);
  display.print(servo_dir > 0 ? ">" : "<");

  display.setCursor(0, 24);
  display.print("P_harvest: ");
  display.print(p_harvest);
  display.println(" mW");

  int barWidth = map(p_harvest, 0, MAX_HARVEST_MW, 0, 60);
  display.drawRect(0, 35, 60, 8, SSD1306_WHITE);
  display.fillRect(1, 36, barWidth, 6, SSD1306_WHITE);
  display.setCursor(63, 36);

  // Display-friendly state label
  if (strcmp(state, "SURPLUS_PLUS") == 0) display.println("++SURPLUS++");
  else display.println(state);

  display.setCursor(0, 45);
  display.print("En. Debt: ");
  display.print(e_debt);
  display.println("mWh");

  display.setCursor(0, 55);
  display.print("Battery Pct.: ");
  display.print(soc);
  display.println(" %");

  display.display();

  // 9. POST to dashboard (non-blocking — runs every loop cycle ~1s)
  connectWiFi();
  postToDashboard(input_val_LDR, p_harvest, e_debt,
                  v_batt, soc, servo_pos, servo_dir, state);

  delay(1000);
}