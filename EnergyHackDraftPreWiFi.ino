// ─────────────────────────────────────────────────────────────────
//  Solar Energy Debt Tracker — LDR + SSD1306 OLED + Servo Tracker
//  ESP32 DevKit v1
//
//  Libraries required (Arduino IDE > Library Manager):
//    - Adafruit SSD1306
//    - Adafruit GFX Library
//    - ESP32Servo
//
//  Wiring:
//    LDR pin 1     ──► 3.3V
//    LDR pin 2     ──► GPIO34  +  10kΩ resistor to GND  (voltage divider)
//    OLED VCC      ──► 3.3V
//    OLED GND      ──► GND
//    OLED SDA      ──► GPIO21
//    OLED SCL      ──► GPIO22
//    Servo signal  ──► GPIO13
//    Servo VCC     ──► 5V (use external supply if servo causes resets)
//    Servo GND     ──► GND (shared with ESP32)
// ─────────────────────────────────────────────────────────────────


#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP32Servo.h>


// ── OLED ──────────────────────────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1        // no reset pin on most modules
#define OLED_ADDRESS 0x3C        // default I2C address for 0.96" OLED

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);


// ── LDR ──────────────────────────────────────────────────────────
int LDR = 34;
int input_val_LDR = 0;


// ── Voltage Sensor ───────────────────────────────────────────────
int BAT_PIN = 35;
float v_batt = 0.0;
float soc    = 0.0;


// ── Energy Debt ──────────────────────────────────────────────────
float e_debt = 0.0;               // running energy balance in mWh
unsigned long last_time = 0;


// ── State thresholds (tune after calibrating your LDR range) ─────
//  Based on your observed range: 1300 (dark) to 4095 (bright)
#define SURPLUS_THRESHOLD  2800   // bright light
#define DEFICIT_THRESHOLD  2150   // dim / covered


// ── Simulated max harvest power (mW) for your 5V/100mA solar cell
#define MAX_HARVEST_MW 500


// ── Servo tracker ────────────────────────────────────────────────
//  Behavior: continuous hill-climbing chase
//    - Always moving one step at a time toward higher LDR values
//    - If LDR drops after a step, reverse direction immediately
//    - When LDR is high (above SETTLE_THRESHOLD), slow step rate
//      so servo makes small corrections rather than hunting
//    - When LDR is low, step rate is fast — actively searching
#define SERVO_PIN              13
#define SERVO_CENTER           90
#define SERVO_HALF_ARC         27    // ±27° = 55° total arc (63°–117°)
#define SERVO_MIN_DEG          (SERVO_CENTER - SERVO_HALF_ARC)  // 63°
#define SERVO_MAX_DEG          (SERVO_CENTER + SERVO_HALF_ARC)  // 117°
#define SERVO_STEP             3     // degrees per step
#define LDR_NOISE_FLOOR        30    // ignore changes smaller than this (ADC noise)
#define SETTLE_THRESHOLD       2800  // LDR above this = light found, slow down steps
#define FAST_STEP_MS           40    // ms between steps when searching (LDR low)
#define SLOW_STEP_MS           300   // ms between steps when near peak (LDR high)

Servo tracker;
int  servo_pos   = SERVO_CENTER;
int  servo_dir   = 1;            // +1 = moving toward higher angles, -1 = toward lower
int  prev_ldr    = 0;
unsigned long last_step_time = 0;


// ── Chase step — call every loop() ──────────────────────────────
//  Moves servo one step in current direction.
//  If LDR dropped vs previous step, flip direction.
//  Step rate adapts: fast when LDR is low, slow when LDR is high.
void runChaseStep(int current_ldr) {
  // Choose step interval based on current light level
  unsigned long step_interval = (current_ldr >= SETTLE_THRESHOLD) ? SLOW_STEP_MS : FAST_STEP_MS;

  if (millis() - last_step_time < step_interval) return;
  last_step_time = millis();

  // Check if last move made things worse — if so, reverse
  if (prev_ldr - current_ldr > LDR_NOISE_FLOOR) {
    servo_dir = -servo_dir;
  }

  // Move one step in current direction
  int next_pos = servo_pos + (servo_dir * SERVO_STEP);

  // Bounce off arc boundaries
  if (next_pos > SERVO_MAX_DEG) {
    next_pos  = SERVO_MAX_DEG;
    servo_dir = -1;             // hit right wall — go left
  } else if (next_pos < SERVO_MIN_DEG) {
    next_pos  = SERVO_MIN_DEG;
    servo_dir = 1;              // hit left wall — go right
  }

  tracker.write(next_pos);
  servo_pos = next_pos;
  prev_ldr  = current_ldr;
}


void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);   // full 0–3.3V range on ADC
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
  delay(1500);

  last_time      = millis();
  prev_ldr       = analogRead(LDR);
  last_step_time = millis();
}


void loop() {

  // ── 1. Read LDR & Voltage Sensor ─────────────────────────────────────────────────
  input_val_LDR = analogRead(LDR);

  v_batt = analogRead(BAT_PIN) * (3.3 / 4095.0) * 5.0;  // scale back up
  soc = map(v_batt * 100, 000, 900, 0, 100);             // 0.0V=0%, 9.0V=100%
  soc = constrain(soc, 0, 100);


  // ── Servo chase — runs every loop, step rate adapts to LDR ───
  runChaseStep(input_val_LDR);


  // ── 2. Map to simulated harvest power ───────────────────────────
  //  floor is ~1300; map that range to 0–MAX_HARVEST_MW
  int p_harvest = map(input_val_LDR, 1300, 4095, 0, MAX_HARVEST_MW);
  p_harvest = constrain(p_harvest, 0, MAX_HARVEST_MW);


  // ── 3. Energy Debt Accumulation ─────────────────────────────────
  float p_load = 150.0;              // for demo purposes, lets assume the system consumes 150mW
  float delta_p = p_harvest - p_load;
  unsigned long now = millis();
  float dt_hours = (now - last_time) / 3600000.0;  // ms → hours
  e_debt += delta_p * dt_hours;     // mWh accumulated
  last_time = now;


  // ── 4. Determine operating state ────────────────────────────────
  const char* state;
  if (input_val_LDR >= SURPLUS_THRESHOLD) {
    state = "++SURPLUS++";
  } else if (input_val_LDR <= DEFICIT_THRESHOLD) {
    state = "DEFICIT";
  } else {
    state = "SURPLUS";
  }


  // ── 5. Serial output ─────────────────────────────────────────────
  Serial.print("LDR Value is: ");
  Serial.print(input_val_LDR);
  Serial.print("  |  P_harvest: ");
  Serial.print(p_harvest);
  Serial.print(" mW  |  Voltage: ");
  Serial.print(v_batt);
  Serial.print("  |  Servo: ");
  Serial.print(servo_pos);
  Serial.print(servo_dir > 0 ? "> " : "< ");
  Serial.println(state);


  // ── 6. OLED layout ──────────────────────────────────────────────
  display.clearDisplay();


  // ── 7. LED Indicator  ───────────────────────────────────────────
  if (strcmp(state, "DEFICIT") == 0) {
    digitalWrite(26, LOW);   // shed load
  } else {
    digitalWrite(26, HIGH);  // load on
  }


  // Row 0: title
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("== Energy Tracker ==");
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);   // divider


  // Row 1: LDR + servo angle + direction
  display.setCursor(0, 13);
  display.print("LDR:");
  display.print(input_val_LDR);
  display.print(" Srv:");
  display.print(servo_pos);
  display.print(servo_dir > 0 ? ">" : "<");


  // Row 2: simulated harvest power
  display.setCursor(0, 24);
  display.print("P_harvest: ");
  display.print(p_harvest);
  display.println(" mW");


  // Row 3: bar graph (harvest meter)
  int barWidth = map(p_harvest, 0, MAX_HARVEST_MW, 0, 60);
  display.drawRect(0, 35, 60, 8, SSD1306_WHITE);          // outline
  display.fillRect(1, 36, barWidth, 6, SSD1306_WHITE);     // fill
  display.setCursor(63, 36);
  display.println(state);


  // Row 4: state label
  display.setCursor(0, 45);
  display.print("En. Debt: ");
  display.print(e_debt);
  display.println("mWh");


  // Row 5: Battery State of Change
  display.setCursor(0, 55);
  display.print("Battery Pct.: ");
  display.print(soc);
  display.println(" %");


  display.display();
  delay(1000);
}