/***************************************************
   Date: 12/4/2025

   This is for
   ESP_32_S3

   ESP32 2.0.13 Board
   ESP32S3 Dev Module
   USB CDC On Boot = DISABLE
   Flash Size = 16MB
   Use Custom Partition   16MB FLASH(3MB APP/9.9MB FATFS)
   PSRAM = OPI PSRAM
   JTAG Adapter = Intergrated USB JTAG

   PINS:
   ESP32S3 SENSORS
   - SCL = 4
   - SDA = 5
   - ANALOG IN PIN1 = 6
   - ANALOG IN PIN2 = 7
   - TXD2 = 8
   - RXD2 = 9
   - ANALOG IN PIN3 = 10
   - ANALOG IN PIN4 = 11
   - ANALOG OUT PIN1 = 12
   - ANALOG OUT PIN2 = 13
   - ANALOG OUT PIN3 = 14
   - NEO PIXEL PIN = 15
   - BUZZER PIN = 16
   - TXD1 = 17
   - RXD1 = 18
   - TXD0 = 43
   - RXD0 = 44

   PINS:
   ESP32S3 LORA
   - SX RESET = 7
   - SX BUSY = 8
   - SX CS = 9
   - SX CLK = 10
   - SX MISO = 11
   - SX MOSI = 12
   - SX DI01 = 13
   - SX DI02 = 14
   - NEO PIXEL PIN = 15
   - TXD1 = 17
   - RXD1 = 18

   

   Color Codes:
        BLACK  = 0
        RED    = 1
        GREEN  = 2
        BLUE   = 3
        WHITE  = 4
        ORANGE  = 5
        YELLOW = 6
        VIOLET = 7
        PINK   = 8

   UPDATE:


 ***************************************************/
//-------------------------------------------------------------------------------------------------------------------------------
//                       THE NeoPixel SETUP
//-------------------------------------------------------------------------------------------------------------------------------

#include <Arduino.h>
#include <Adafruit_NeoPixel.h> // include Adafruit_NeoPixel.h library

#define PIN 15 //Pin NeoPixel
#define BUZZER_PIN   16 //Pin Buzzer

#define NUMLEDS 1 //Numero de LEDs
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(NUMLEDS, PIN, NEO_GRB  + NEO_KHZ800);

const uint32_t colors[] = {
  pixels.Color(0, 0, 0), //  BLACK
  pixels.Color(255, 0, 0), //RED
  pixels.Color(0, 255, 0), //GREEN
  pixels.Color(0, 0, 255), //BLU
  pixels.Color(255, 255, 255), // WHITE
  pixels.Color(255, 121, 0), // ORAGNE
  pixels.Color(255, 255, 0), // YELLOW
  pixels.Color(143, 0, 255), // VIOLET
  pixels.Color(255, 20, 147), // PINK
};



//-------------------------------------------------------------------------------------------------------------------------------
//                       THE SETUP
//-------------------------------------------------------------------------------------------------------------------------------

void setup() {

  Serial.begin(115200);

  // Setup Buzzer Pin
  pinMode(BUZZER_PIN, OUTPUT);

  pixels.begin(); // INITIALIZE NeoPixel strip object (REQUIRED)
  pixels.setBrightness(50); // Set brightness (0-255) to avoid drawing too much current

  pixels.clear(); // Set all pixel colors to 'off'
  pixels.setPixelColor(0, colors[2]);
  pixels.show();   // Send the updated pixel colors to the hardware.

  // --- BUZZER LOOP (3 Times) ---
  // We use analogWrite to generate a PWM signal (tone)
  for(int i = 0; i < 3; i++) {
    // 128 is ~50% duty cycle. 
    // If you have a passive buzzer, this creates the tone.
    // If you have an active buzzer, this acts like "ON".
    analogWrite(BUZZER_PIN, 200); 
    delay(200); // Beep duration
    
    analogWrite(BUZZER_PIN, 0);   // Turn off
    delay(100); // Silence duration
  }

  analogWrite(BUZZER_PIN, 0);   // Turn off
}



//---------------------------------------------------- VOID MAIN LOOP -------------------------------------------------------------------

void loop() {

//  pixels.clear(); // Set all pixel colors to 'off'
//  pixels.setPixelColor(0, colors[2]);
//  pixels.show();   // Send the updated pixel colors to the hardware.

  delay(100);
}
