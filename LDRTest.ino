int LDR = 34; //declare Light Dependent Resistor (LDR)
int input_val_LDR = 0; //dummy input val

void setup()
{
  Serial.begin(115200);
  analogSetAttenuation(ADC_11db);  // full 0–3.3V range
}

void loop()
{
  input_val = analogRead(LDR);
  Serial.print("LDR Value is: "); 
  Serial.println(input_val_LDR);  //working LDR Value: 1300<LDR<4095
  delay(1000);
}