#include <Arduino.h>
#include <NeoPixelBus.h>

#define NEOPIXEL_PIN 15
#define BUZZER_PIN 16

NeoPixelBus<NeoGrbFeature, Neo800KbpsMethod> strip(1, NEOPIXEL_PIN);

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\nMycoBrain - Minimal NeoPixel Test");
  Serial.println("If you see this, firmware is running!");
  Serial.flush();
  
  // Initialize NeoPixel
  strip.Begin();
  strip.Show();
  
  // Turn NeoPixel GREEN
  strip.SetPixelColor(0, RgbColor(0, 255, 0));
  strip.Show();
  
  Serial.println("NeoPixel should be GREEN now");
  Serial.flush();
  
  // Buzzer test
  pinMode(BUZZER_PIN, OUTPUT);
  for (int i = 0; i < 3; i++) {
    tone(BUZZER_PIN, 1000, 200);
    delay(300);
  }
  
  Serial.println("Startup jingle played!");
  Serial.println("Ready!");
  Serial.flush();
}

void loop() {
  static unsigned long lastPrint = 0;
  unsigned long now = millis();
  
  if (now - lastPrint >= 5000) {
    Serial.print("Alive: ");
    Serial.println(now / 1000);
    Serial.flush();
    lastPrint = now;
  }
  
  delay(100);
}
