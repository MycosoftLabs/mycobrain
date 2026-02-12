/*
 * MycoBrain Side-A Test Firmware
 * Ultra-simple test to verify board is working
 */

#include <Arduino.h>

#define NEOPIXEL_PIN 15
#define BUZZER_PIN 16

void setup() {
  Serial.begin(115200);
  delay(2000);  // Wait for Serial Monitor
  
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(NEOPIXEL_PIN, OUTPUT);
  
  Serial.println("\n\n========================================");
  Serial.println("MycoBrain Side-A Test Firmware");
  Serial.println("========================================");
  Serial.println("If you see this, Serial is working!");
  Serial.println("Commands: coin, bump, 1up, morgio");
  Serial.println("          led rgb <r> <g> <b>");
  Serial.println("Ready!");
  Serial.flush();
  
  // Startup beep
  tone(BUZZER_PIN, 1000, 200);
  delay(300);
  
  // Blink LED 3 times
  for (int i = 0; i < 3; i++) {
    digitalWrite(NEOPIXEL_PIN, HIGH);
    delay(100);
    digitalWrite(NEOPIXEL_PIN, LOW);
    delay(100);
  }
  
  Serial.println("Startup complete!");
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    
    Serial.print("Received: ");
    Serial.println(cmd);
    
    if (cmd == "coin") {
      tone(BUZZER_PIN, 800, 100);
      delay(100);
      tone(BUZZER_PIN, 1000, 100);
      Serial.println("OK: coin");
    }
    else if (cmd == "bump") {
      tone(BUZZER_PIN, 200, 50);
      Serial.println("OK: bump");
    }
    else if (cmd == "1up") {
      tone(BUZZER_PIN, 523, 150);
      delay(150);
      tone(BUZZER_PIN, 659, 150);
      delay(150);
      tone(BUZZER_PIN, 784, 150);
      Serial.println("OK: 1up");
    }
    else if (cmd == "morgio") {
      tone(BUZZER_PIN, 440, 100);
      delay(100);
      tone(BUZZER_PIN, 554, 100);
      delay(100);
      tone(BUZZER_PIN, 659, 200);
      Serial.println("OK: morgio");
    }
    else if (cmd.startsWith("led rgb")) {
      int r = 0, g = 0, b = 0;
      sscanf(cmd.c_str(), "led rgb %d %d %d", &r, &g, &b);
      if (r > 0 || g > 0 || b > 0) {
        digitalWrite(NEOPIXEL_PIN, HIGH);
        Serial.print("OK: led rgb ");
        Serial.print(r);
        Serial.print(" ");
        Serial.print(g);
        Serial.print(" ");
        Serial.println(b);
      } else {
        digitalWrite(NEOPIXEL_PIN, LOW);
        Serial.println("OK: led off");
      }
    }
    else if (cmd == "led off") {
      digitalWrite(NEOPIXEL_PIN, LOW);
      Serial.println("OK: led off");
    }
    else {
      Serial.print("ERROR: Unknown: ");
      Serial.println(cmd);
    }
  }
  
  delay(10);
}
