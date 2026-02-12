/*
 * MycoBrain Side-A - ULTRA MINIMAL - Serial Test Only
 * Test Serial first, then add hardware
 */

#include <Arduino.h>

void setup() {
  // Initialize Serial IMMEDIATELY - no delays
  Serial.begin(115200);
  
  // Print immediately
  Serial.println("\n\nBOOT START");
  Serial.flush();
  delay(100);
  
  Serial.println("Serial initialized");
  Serial.flush();
  delay(100);
  
  Serial.println("Testing hardware...");
  Serial.flush();
  delay(100);
  
  // Test NeoPixel GPIO15
  pinMode(15, OUTPUT);
  Serial.println("GPIO15 set as OUTPUT");
  Serial.flush();
  delay(100);
  
  // Test Buzzer GPIO16
  pinMode(16, OUTPUT);
  Serial.println("GPIO16 set as OUTPUT");
  Serial.flush();
  delay(100);
  
  Serial.println("Setup complete!");
  Serial.println("If you see this, firmware is NOT crashing!");
  Serial.flush();
}

void loop() {
  static unsigned long lastPrint = 0;
  unsigned long now = millis();
  
  // Print every 2 seconds
  if (now - lastPrint >= 2000) {
    Serial.print("Loop running: ");
    Serial.print(now / 1000);
    Serial.println(" seconds");
    Serial.flush();
    lastPrint = now;
  }
  
  delay(100);
  yield();
}
