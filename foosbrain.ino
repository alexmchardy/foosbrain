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

// Unused analog pin for seeding random number generator
#define UNUSED_ANALOG_PIN 0

// Pin 13: Arduino has an LED connected on pin 13
#define LEDPIN 13

// File reading/writing
#include <strings.h>
char buf[50] = "";
const String ROOT = "foos/";
const String SETTINGS_FILENAME = "settings.txt";
String fileLine;
String themeDir;

// Setup goal detection
#define GOAL_PIN_BLACK 8
#define GOAL_PIN_YELLOW 9
#define GOAL_MS_SLOW 20
#define GOAL_MS_FAST 10

uint32_t blackGoalStartTime;
volatile uint32_t blackGoalTime = 0;
uint32_t yellowGoalStartTime;
volatile uint32_t yellowGoalTime = 0;

void goalSensorChange() {
    uint32_t *startTime;
    volatile uint32_t *goalTime;
    if (PCintPort::arduinoPin == GOAL_PIN_BLACK) {
        startTime = &blackGoalStartTime;
        goalTime = &blackGoalTime;
    } else {
        startTime = &yellowGoalStartTime;
        goalTime = &yellowGoalTime;
    }
    if (PCintPort::pinState == LOW) {
        *startTime = millis();
    } else {
        *goalTime = millis() - *startTime;
    }
}

void setup() {

    Serial.begin(9600);

    if (! musicPlayer.begin()) { // initialise the music player
        Serial.println(F("Couldn't find VS1053, do you have the right pins defined?"));
        while (1);
    }
    Serial.println(F("VS1053 found"));

    // Initialize the SD card
    initSoundFiles();

    // Set volume for left, right channels. lower numbers == louder volume!
    musicPlayer.setVolume(20,20);

    // If DREQ is on an interrupt pin (on uno, #2 or #3) we can do background audio playing
    musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int

    // initialize the LED pin as an output:
    pinMode(LEDPIN, OUTPUT);

    // Set the goal sensors pins
    digitalWrite(GOAL_PIN_BLACK, INPUT_PULLUP);
    digitalWrite(GOAL_PIN_YELLOW, INPUT_PULLUP);

    // Attach goal sensor interrupts
    attachPinChangeInterrupt(GOAL_PIN_BLACK, goalSensorChange, CHANGE);
    attachPinChangeInterrupt(GOAL_PIN_YELLOW, goalSensorChange, CHANGE);

    // Make a tone to indicate we're ready to roll
    musicPlayer.sineTest(0x44, 500);
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

void initSoundFiles() {
    SD.begin(CARDCS);
    ROOT.toCharArray(buf, 50);
    if (!SD.exists(buf)) {
        Serial.println(F("Error: foos/ dir not found on SD card"));
    }
    (ROOT+SETTINGS_FILENAME).toCharArray(buf, 50);
    File settingsFile = SD.open(buf, FILE_READ);
    if (!settingsFile.available()) {
        Serial.println("settings.txt not available");
    }
    while (settingsFile.available()) {
        fileLine = readline(settingsFile);
        if (fileLine.startsWith("themeDir:")) {
            themeDir = fileLine.substring(9);
        }
        Serial.println(fileLine);
    }
    settingsFile.close();
    if (themeDir == "") {
        Serial.println("no themeDir found");
        settingsFile = SD.open(buf, FILE_WRITE);
        themeDir = "Simpsons";
        settingsFile.println("themeDir:" + themeDir);
        settingsFile.close();
    }
}

String readline(File file) {
    String line = "";
    char c;
    while (file.available()) {
        c = (char)file.read();
        if (c == '\n') {
            break;
        }
        line += c;
    }
    return line;
}
