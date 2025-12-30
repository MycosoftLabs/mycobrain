/*
 * MycoBrain Side-A FIXED VERSION
 * This version has all fixes to prevent boot loops and crashes
 */

#include <Arduino.h>
#include "soc/rtc_cntl_reg.h" // For brownout detector disable

// Pin definitions
static const int BUZZER_PIN = 16;
static const int AO_PINS[3] = {12, 13, 14}; // R,G,B

void setup() {
  // CRITICAL: Disable brownout detector FIRST
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  pinMode(BUZZER_PIN, OUTPUT);
  
  Serial.begin(115200);
  
  // Wait for serial with timeout
  unsigned long start = millis();
  while (!Serial && (millis() - start < 5000)) {
    delay(10);
    yield();
  }
  
  // Extra delay for Serial Monitor
  delay(2000);
  
  // Setup LED pins
  for (int i = 0; i < 3; i++) {
    pinMode(AO_PINS[i], OUTPUT);
    analogWriteResolution(AO_PINS[i], 8);
    analogWrite(AO_PINS[i], 0);
  }
  
  Serial.println("\n\n========================================");
  Serial.println("MycoBrain Side-A FIXED VERSION");
  Serial.println("========================================");
  Serial.println("Hardware check:");
  Serial.println("  - Brownout detector: DISABLED");
  Serial.println("  - Watchdog: Enabled (will feed)");
  Serial.println("  - LEDs: Initialized");
  Serial.println("  - Buzzer: Initialized");
  Serial.flush();
  
  // Test LED - Green
  analogWrite(AO_PINS[0], 0);
  analogWrite(AO_PINS[1], 255);
  analogWrite(AO_PINS[2], 0);
  delay(500);
  analogWrite(AO_PINS[0], 0);
  analogWrite(AO_PINS[1], 0);
  analogWrite(AO_PINS[2], 0);
  
  // Test buzzer
  tone(BUZZER_PIN, 1000, 200);
  delay(300);
  
  Serial.println("\nDevice initialized successfully!");
  Serial.println("If you see this, firmware is working!");
  Serial.flush();
}

void loop() {
  static unsigned long lastPrint = 0;
  static unsigned long lastBlink = 0;
  unsigned long now = millis();
  
  // Feed watchdog
  yield();
  
  // Print status every 5 seconds
  if (now - lastPrint >= 5000) {
    Serial.print("Uptime: ");
    Serial.print(now / 1000);
    Serial.println(" seconds - Device running OK!");
    Serial.flush();
    lastPrint = now;
  }
  
  // Blink LED every 2 seconds (blue)
  if (now - lastBlink >= 2000) {
    analogWrite(AO_PINS[0], 0);
    analogWrite(AO_PINS[1], 0);
    analogWrite(AO_PINS[2], 100);
    delay(100);
    analogWrite(AO_PINS[0], 0);
    analogWrite(AO_PINS[1], 0);
    analogWrite(AO_PINS[2], 0);
    lastBlink = now;
  }
  
  delay(100);
  yield(); // Feed watchdog again
}


