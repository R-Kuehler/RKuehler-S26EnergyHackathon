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

// ── State thresholds (tune after calibrating your LDR range) ─────
//  Based on your observed range: 1300 (dark) to 4095 (bright)
#define SURPLUS_THRESHOLD  3200   // bright light
#define DEFICIT_THRESHOLD  1800   // dim / covered

// ── Simulated max harvest power (mW) for your 5V/100mA solar cell
#define MAX_HARVEST_MW 500

void setup() {
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);   // full 0–3.3V range on ADC

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
  // ── 1. Read LDR ─────────────────────────────────────────────────
  input_val_LDR = analogRead(LDR);

  // ── 2. Map to simulated harvest power ───────────────────────────
  //  Your LDR floor is ~1300; map that range to 0–MAX_HARVEST_MW
  int p_harvest = map(input_val_LDR, 1300, 4095, 0, MAX_HARVEST_MW);
  p_harvest = constrain(p_harvest, 0, MAX_HARVEST_MW);

  // ── 3. Determine operating state ────────────────────────────────
  const char* state;
  if (input_val_LDR >= SURPLUS_THRESHOLD) {
    state = "SURPLUS";
  } else if (input_val_LDR <= DEFICIT_THRESHOLD) {
    state = "DEFICIT";
  } else {
    state = "BALANCED";
  }

  // ── 4. Serial output ─────────────────────────────────────────────
  Serial.print("LDR Value is: ");
  Serial.print(input_val_LDR);
  Serial.print("  |  P_harvest: ");
  Serial.print(p_harvest);
  Serial.print(" mW  |  State: ");
  Serial.println(state);

  // ── 5. OLED layout ──────────────────────────────────────────────
  display.clearDisplay();

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
  int barWidth = map(p_harvest, 0, MAX_HARVEST_MW, 0, 110);
  display.drawRect(0, 35, 112, 8, SSD1306_WHITE);          // outline
  display.fillRect(1, 36, barWidth, 6, SSD1306_WHITE);     // fill

  // Row 4: large state label
  display.setTextSize(2);
  display.setCursor(0, 48);
  display.print(state);

  display.display();
  delay(1000);
}