/*
 * MycoBrain Side-A Minimal Working Firmware
 * Simple plaintext CLI with NeoPixel and Buzzer
 * GPIO15: NeoPixel (SK6805)
 * GPIO16: Buzzer
 */

#include <Arduino.h>

#define NEOPIXEL_PIN 15
#define BUZZER_PIN 16
#define SERIAL_BAUD 115200

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(1000);
  
  // Initialize pins
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  
  pinMode(NEOPIXEL_PIN, OUTPUT);
  digitalWrite(NEOPIXEL_PIN, LOW);
  
  Serial.println("\n========================================");
  Serial.println("MycoBrain Side-A Minimal Firmware");
  Serial.println("========================================");
  Serial.println("Commands:");
  Serial.println("  coin, bump, 1up, morgio, power");
  Serial.println("  led rgb <r> <g> <b>");
  Serial.println("  led off");
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
}

void loop() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    cmd.toLowerCase();
    
    // Buzzer commands
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
    else if (cmd == "power") {
      tone(BUZZER_PIN, 600, 200);
      Serial.println("OK: power");
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
    // LED commands
    else if (cmd.startsWith("led rgb")) {
      int r = 0, g = 0, b = 0;
      sscanf(cmd.c_str(), "led rgb %d %d %d", &r, &g, &b);
      // Simple digital control - turn on if any color > 0
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
    else if (cmd == "help") {
      Serial.println("Commands: coin, bump, 1up, morgio, power");
      Serial.println("          led rgb <r> <g> <b>");
      Serial.println("          led off");
    }
    else {
      Serial.print("ERROR: Unknown command: ");
      Serial.println(cmd);
    }
  }
  
  delay(10);
}
