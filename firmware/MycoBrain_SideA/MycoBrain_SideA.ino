/*
 * MINIMAL TEST - MycoBrain Side-A
 * This is the simplest possible firmware to test if hardware is OK
 * If this works, the issue is in sensor/BSEC code
 * If this doe1n't work, there's a hardware problem
 */

#include "soc/rtc_cntl_reg.h" // For brownout detector disable

void setup() {
  // Disable brownout detector
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  
  // Wait for serial
  unsigned long start = millis();
  while (!Serial && (millis() - start < 5000)) {
    delay(10);
  }
  
  delay(2000); // Extra delay for Serial Monitor
  
  Serial.println("\n\n========================================");
  Serial.println("MINIMAL TEST - Hardware Check");
  Serial.println("========================================");
  Serial.println("If you see this, hardware is OK!");
  Serial.println("Device is NOT resetting.");
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
  yield(); // Feed watchdog
}

