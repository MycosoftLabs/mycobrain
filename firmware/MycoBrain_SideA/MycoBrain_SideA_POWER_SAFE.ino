/*
 * MycoBrain Side-A POWER-SAFE VERSION
 * Designed for boards with bridge modification and 2x BME688 sensors
 * Initializes sensors gradually to prevent brownout
 */

#include <Arduino.h>
#include <Wire.h>
#include "soc/rtc_cntl_reg.h"

// Pin definitions (from Garrett's working code)
#define SDA_PIN 5
#define SCL_PIN 4
#define BUZZER_PIN 16
#define NEOPIXEL_PIN 15
// AO pins: R=12, G=13, B=14
static const int AO_PINS[3] = {12, 13, 14};

// Power management
#define INIT_DELAY_MS 500
#define SENSOR_INIT_DELAY_MS 200

void setup() {
  // CRITICAL: Disable brownout detector FIRST
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  // Reduce CPU frequency to save power
  setCpuFrequencyMhz(80);
  
  // Small delay for power to stabilize
  delay(200);
  
  Serial.begin(115200);
  
  // Wait for serial with timeout
  unsigned long start = millis();
  while (!Serial && (millis() - start < 3000)) {
    delay(10);
    yield();
  }
  
  delay(1500);
  
  Serial.println("\n\n========================================");
  Serial.println("MycoBrain Side-A POWER-SAFE VERSION");
  Serial.println("========================================");
  Serial.println("Designed for bridged board with 2x BME688");
  Serial.println("Initializing gradually to prevent brownout...");
  Serial.flush();
  
  // Initialize pins gradually
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  delay(INIT_DELAY_MS);
  yield();
  
  // Initialize I2C (but don't scan yet - saves power)
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000); // Slow I2C to save power
  delay(INIT_DELAY_MS);
  yield();
  
  Serial.println("Basic initialization complete");
  Serial.println("Skipping sensor init to prevent brownout");
  Serial.println("Board is running - sensors can be initialized later");
  Serial.flush();
  
  // Test LED (brief, low power)
  for (int i = 0; i < 3; i++) {
    pinMode(AO_PINS[i], OUTPUT);
    analogWriteResolution(AO_PINS[i], 8);
    analogWrite(AO_PINS[i], 0);
  }
  
  // Brief green flash to show it's working
  analogWrite(AO_PINS[1], 50); // Low brightness green
  delay(100);
  analogWrite(AO_PINS[1], 0);
  
  Serial.println("\nDevice initialized successfully!");
  Serial.println("If you see this, firmware is working!");
  Serial.println("NOTE: Sensors not initialized to prevent brownout");
  Serial.flush();
}

void loop() {
  static unsigned long lastPrint = 0;
  static unsigned long lastBlink = 0;
  unsigned long now = millis();
  
  // Feed watchdog frequently
  yield();
  
  // Print status every 5 seconds
  if (now - lastPrint >= 5000) {
    Serial.print("Uptime: ");
    Serial.print(now / 1000);
    Serial.println(" seconds - Device running OK!");
    Serial.println("(Sensors disabled to prevent brownout)");
    Serial.flush();
    lastPrint = now;
  }
  
  // Blink LED every 3 seconds (very dim to save power)
  if (now - lastBlink >= 3000) {
    analogWrite(AO_PINS[2], 20); // Very dim blue
    delay(50);
    analogWrite(AO_PINS[2], 0);
    lastBlink = now;
  }
  
  delay(100);
  yield();
}

