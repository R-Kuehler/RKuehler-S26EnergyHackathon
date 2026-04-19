// ─────────────────────────────────────────────────────────────────
//  Solar Energy Debt Tracker — LDR + SSD1306 OLED
//  ESP32 DevKit v1
//
//  Libraries required (Arduino IDE > Library Manager):
//    - Adafruit SSD1306
//    - Adafruit GFX Library
//
//  Wiring:
//    LDR pin 1  ──► 3.3V
//    LDR pin 2  ──► GPIO34  +  10kΩ resistor to GND  (voltage divider)
//    OLED VCC   ──► 3.3V
//    OLED GND   ──► GND
//    OLED SDA   ──► GPIO21
//    OLED SCL   ──► GPIO22
// ─────────────────────────────────────────────────────────────────

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

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
float soc = 0.0;

// ── Energy Debt ──────────────────────────────────────────────────
float e_debt = 0.0;               // running energy balance in mWh
unsigned long last_time = 0;

// ── State thresholds (tune after calibrating your LDR range) ─────
//  Based on your observed range: 1300 (dark) to 4095 (bright)
#define SURPLUS_THRESHOLD  2800   // bright light
#define DEFICIT_THRESHOLD  2150   // dim / covered

// ── Simulated max harvest power (mW) for your 5V/100mA solar cell
#define MAX_HARVEST_MW 500

void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);   // full 0–3.3V range on ADC
  pinMode(26, OUTPUT);

  // Initialise OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
    Serial.println("SSD1306 not found — check wiring & I2C address");
    while (true);                   // halt; fix wiring before continuing
  }

  // Splash screen
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(20, 20);
  display.println("Energy Tracker");
  display.setCursor(30, 35);
  display.println("Starting...");
  display.display();
  delay(1500);
}

void loop() {

  // ── 1. Read LDR & Voltage Sensor ─────────────────────────────────────────────────
  input_val_LDR = analogRead(LDR);

  v_batt = analogRead(BAT_PIN) * (3.3 / 4095.0) * 5.0;  // scale back up
  soc = map(v_batt * 100, 000, 900, 0, 100);             // 0.0V=0%, 9.0V=100%
  soc = constrain(soc, 0, 100);

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
  Serial.println(v_batt);


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

  // Row 1: LDR raw value
  display.setCursor(0, 13);
  display.print("LDR Raw:   ");
  display.println(input_val_LDR);

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

  //Row 5: Battery State of Change
  display.setCursor(0, 55);
  display.print("Battery Pct.: ");
  display.print(soc);
  display.println(" %");

  display.display();
  delay(1000);
}