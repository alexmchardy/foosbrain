/* 
  IR Breakbeam with Mp3
*/
// include PinChangeInt (with optimization switches)
#define NO_PORTC_PINCHANGES // to indicate that port c will not be used for pin change interrupts
#define NO_PORTD_PINCHANGES // to indicate that port d will not be used for pin change interrupts
#include <PinChangeInt.h>

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

// Pin 13: Arduino has an LED connected on pin 13
#define LEDPIN 13

// Setup goal detection
#define BLACKGOALPIN 8
#define YELLOWGOALPIN 9

unsigned long blackGoalStartTime;
//unsigned long blackGoalEndTime;
volatile unsigned long blackGoalTime = 0;
unsigned long yellowGoalStartTime;
//unsigned long yellowGoalEndTime;
volatile unsigned long yellowGoalTime = 0;

void goalSensorChange() {
    unsigned long *startTime;
    //unsigned long *endTime;
    volatile unsigned long *goalTime;
    if (PCintPort::arduinoPin == BLACKGOALPIN) {
        startTime = &blackGoalStartTime;
        //endTime = &blackGoalEndTime;
        goalTime = &blackGoalTime;
    } else {
        startTime = &yellowGoalStartTime;
        //endTime = &yellowGoalEndTime;
        goalTime = &yellowGoalTime;
    }
    if (PCintPort::pinState == LOW) {
        *startTime = millis();
    } else {
        //*endTime = millis();
        *goalTime = millis() - *startTime;
    }
/*
    if (PCintPort::arduinoPin == BLACKGOALPIN) {
        if (PCintPort::pinState == LOW) {
            blackGoalStartTime = millis();
        } else {
            blackGoalEndTime = millis();
            blackGoalTime = blackGoalEndTime - blackGoalStartTime;
        }
    } else {
        if (PCintPort::pinState == LOW) {
            yellowGoalStartTime = millis();
        } else {
            yellowGoalEndTime = millis();
            yellowGoalTime = yellowGoalEndTime - yellowGoalStartTime;
        }
    }
*/
}

void setup() {

    Serial.begin(9600);

    if (! musicPlayer.begin()) { // initialise the music player
        Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
        while (1);
    }
    Serial.println(F("VS1053 found"));

    musicPlayer.sineTest(0x44, 500);    // Make a tone to indicate VS1053 is working

    SD.begin(CARDCS);    // initialise the SD card

    // Set volume for left, right channels. lower numbers == louder volume!
    musicPlayer.setVolume(20,20);

    // If DREQ is on an interrupt pin (on uno, #2 or #3) we can do background
    // audio playing
    musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int

    // initialize the LED pin as an output:
    pinMode(LEDPIN, OUTPUT);

    // Set the goal sensors pins
    digitalWrite(BLACKGOALPIN, INPUT_PULLUP);
    digitalWrite(YELLOWGOALPIN, INPUT_PULLUP);

    // Attach goal sensor interrupts
    attachPinChangeInterrupt(BLACKGOALPIN, goalSensorChange, CHANGE);
    attachPinChangeInterrupt(YELLOWGOALPIN, goalSensorChange, CHANGE);
}

void loop(){
    if (blackGoalTime > 0 || yellowGoalTime > 0) {
        if (blackGoalTime) {
            Serial.print(F("Black Goal: ")); Serial.print(blackGoalTime); Serial.println(F(" ms"));
            blackGoalTime = 0;
            musicPlayer.setVolume(20,254);
            musicPlayer.startPlayingFile("track001.mp3");
        }
        if (yellowGoalTime) {
            Serial.print(F("Yellow Goal: ")); Serial.print(yellowGoalTime); Serial.println(F(" ms"));
            yellowGoalTime = 0;
            musicPlayer.setVolume(254,20);
            musicPlayer.startPlayingFile("track002.mp3");
        }
    }
}
