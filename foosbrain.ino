/* 
  IR Breakbeam with Mp3
*/

// include SPI, MP3 and SD libraries
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SD.h>

// These are the pins used for the music maker shield
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)
// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin
// Create shield-example object!
Adafruit_VS1053_FilePlayer musicPlayer = 
  Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

#define LEDPIN 13
  // Pin 13: Arduino has an LED connected on pin 13
  // Pin 11: Teensy 2.0 has the LED on pin 11
  // Pin  6: Teensy++ 2.0 has the LED on pin 6
  // Pin 13: Teensy 3.0 has the LED on pin 13

#define SENSORPIN 2

// Indicates if interrupt has been triggered
volatile int change = 0;
// IR Breakbeam state; LOW == broken
volatile int sensorState = LOW;

void changedState() {
  change = 1;
  // read the state of the sensor
  sensorState = digitalRead(SENSORPIN);
}

void setup() {

  Serial.begin(9600);

  if (! musicPlayer.begin()) { // initialise the music player
     Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
     while (1);
  }
  Serial.println(F("VS1053 found"));
  
  SD.begin(CARDCS);    // initialise the SD card
  
  // Set volume for left, right channels. lower numbers == louder volume!
  musicPlayer.setVolume(20,20);

  // If DREQ is on an interrupt pin (on uno, #2 or #3) we can do background
  // audio playing
  musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int

  // initialize the LED pin as an output:
  pinMode(LEDPIN, OUTPUT);
  // turn on the pullup
  digitalWrite(SENSORPIN, HIGH);
  // attach ISR to the sensor pin
  attachInterrupt(0, changedState, CHANGE);   
}

void loop(){
  if (change) {
    change = 0;
    // check if the sensor beam is broken
    // if it is, the sensorState is LOW:
    if (sensorState == LOW) {
      // turn LED on:
      digitalWrite(LEDPIN, HIGH);
      if (musicPlayer.stopped()) {
        musicPlayer.playFullFile("track002.mp3");
      }
      Serial.println("Broken");
      //tone(8, 362, 25);
    } else {
      // turn LED off:
      digitalWrite(LEDPIN, LOW);
      Serial.println("Unbroken");
    }
  }
}
