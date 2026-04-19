#include <WiFi.h>

const char* ssid     = "SolarTrack";
const char* password = "solartracker123";   

void setup() {
  Serial.begin(115200);
  WiFi.softAP(ssid, password);
  Serial.println("AP started. IP: " + WiFi.softAPIP().toString());
  // Default IP will be 192.168.4.1
}

void loop() {

}