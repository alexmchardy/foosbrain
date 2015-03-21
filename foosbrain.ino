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
const uint8_t NAMELEN = 13;
const uint8_t FILELINELEN = 30;
const uint8_t GOALROOTLEN = 40; // 3*12 + 3 ("/"s) + 1 ("\0")
const uint8_t GOALPATHLEN = 53; // 4*12 + 4 ("/"s) + 1 ("\0")
const uint8_t THEMEFILEPATHLEN = 53; // 4*12 + 4 ("/"s) + 1 ("\0")
const uint8_t GOALFILEPATHLEN = 66; // 5*12 + 5 ("/"s) + 1 ("\0")
const char ROOT[6] = "foos/";
const char SETTINGSFILENAME[13] = "settings.txt";
const char THEMESDIR[8] = "themes/";
const char THEMEFILENAME[11] = "/theme.txt";
const char GOALDIR[7] = "/goal/";
const char FASTGOALDIR[11] = "/goalfast/";
const char SLOWGOALDIR[11] = "/goalslow/";
bool fastGoalDirExists = false;
bool slowGoalDirExists = false;
char themeDir[NAMELEN];
char themeFilePath[THEMEFILEPATHLEN];
char goalRoot[GOALROOTLEN];
char goalPath[GOALPATHLEN];
char fastGoalPath[GOALPATHLEN];
char slowGoalPath[GOALPATHLEN];
char goalFilePath[GOALFILEPATHLEN];
char fastGoalFilePath[GOALFILEPATHLEN];
char slowGoalFilePath[GOALFILEPATHLEN];
uint32_t goalSoundDelay = 0;
uint16_t *goalSoundIndices = NULL;
uint16_t *fastGoalSoundIndices = NULL;
uint16_t *slowGoalSoundIndices = NULL;
uint8_t goalSoundCount;
uint8_t fastGoalSoundCount;
uint8_t slowGoalSoundCount;

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

    // Set volume for left, right channels. lower numbers == louder volume!
    musicPlayer.setVolume(20,20);

    // If DREQ is on an interrupt pin (on uno, #2 or #3) we can do background audio playing
    musicPlayer.useInterrupt(VS1053_FILEPLAYER_PIN_INT);  // DREQ int

    // Initialize the SD card
    initSoundFiles();

/*
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
*/
}

void loop(){
    String goalSoundFile;
    if (blackGoalTime > 0 || yellowGoalTime > 0) {
        if (blackGoalTime) {
            Serial.print(F("Black Goal: ")); Serial.print(blackGoalTime); Serial.println(F(" ms"));
            //goalSoundFile = getGoalSoundFile(blackGoalTime);
            blackGoalTime = 0;
            musicPlayer.setVolume(20,254);
            //musicPlayer.startPlayingFile(goalSoundFile.c_str());
        }
        if (yellowGoalTime) {
            Serial.print(F("Yellow Goal: ")); Serial.print(yellowGoalTime); Serial.println(F(" ms"));
            //goalSoundFile = getGoalSoundFile(yellowGoalTime);
            yellowGoalTime = 0;
            musicPlayer.setVolume(254,20);
            //musicPlayer.startPlayingFile(goalSoundFile.c_str());
        }
    }
}

/*
void getGoalSoundFile(char *goalFile, uint32_t goalTime) {
    if (fastGoalDirExists && goalTime < 10) {
        goalFile = FASTGOALDIR;
    } else if (slowGoalDirExists && goalTime > 30) {
        goalDir = SLOWGOALDIR;
    }
    String fileName = goalSounds[random(1, goalSounds[0].toInt())];
}
*/

void initSoundFiles() {
    SD.begin(CARDCS);

    strncpy(goalRoot, ROOT, GOALROOTLEN-1);
    if (!SD.exists(goalRoot)) {
        Serial.println(F("Error: foos/ dir not found on SD card"));
    }
    strncat(goalRoot, SETTINGSFILENAME, GOALROOTLEN - strlen(goalRoot) - 1);
    File settingsFile = SD.open(goalRoot, FILE_READ);
    if (!settingsFile.available()) {
        Serial.println(F("settings.txt not found"));
    }
    char fileLine[FILELINELEN];
    while (settingsFile.available()) {
        readline(fileLine, settingsFile, FILELINELEN-1);
        if (strncmp(fileLine, "themeDir:", 9) == 0) {
            strncpy(themeDir, fileLine+9, NAMELEN-1);
        } else if (strncmp(fileLine, "goalSoundDelay:", 15) == 0) {
            char numStr[6];
            goalSoundDelay = atoi(strncpy(numStr, fileLine+15, 5));
            Serial.print(F("the goalSoundDelay is: "));
            Serial.println(goalSoundDelay);
        }
    }
    settingsFile.close();

    // Build goal root
    strncpy(goalRoot, ROOT, GOALROOTLEN-1);
    strncat(goalRoot, THEMESDIR, GOALROOTLEN - strlen(goalRoot) - 1);
    strncat(goalRoot, themeDir, GOALROOTLEN - strlen(goalRoot) - 1);

    // Read the theme.txt file
    readTheme(goalRoot);

    // Build regular goal path
    strncpy(goalPath, goalRoot, GOALPATHLEN-1);
    strncat(goalPath, GOALDIR, GOALPATHLEN - strlen(goalPath) - 1);
    if (goalSoundCount == 0 || !SD.exists(goalPath)) {
        Serial.print(F("goal path not found: "));
        Serial.println(goalPath);
        return;
    }
    // Get a random regular goal file path
    randomSeed(analogRead(UNUSED_ANALOG_PIN));
    getSoundFilePath(goalFilePath, goalPath, goalSoundIndices[random(0, goalSoundCount-1)]);

    // Build fast goal path
    if (fastGoalSoundCount > 0) {
        strncpy(fastGoalPath, goalRoot, GOALPATHLEN-1);
        strncat(fastGoalPath, FASTGOALDIR, GOALPATHLEN - strlen(fastGoalPath) - 1);
        if (fastGoalDirExists = SD.exists(fastGoalPath)) {
            // Get a random fast goal file path
            getSoundFilePath(fastGoalFilePath, fastGoalPath, fastGoalSoundIndices[random(0, fastGoalSoundCount-1)]);
        }
    }

    // Build slow goal path
    if (slowGoalSoundCount > 0) {
        strncpy(slowGoalPath, goalRoot, GOALPATHLEN-1);
        strncat(slowGoalPath, SLOWGOALDIR, GOALPATHLEN - strlen(slowGoalPath) - 1);
        if (slowGoalDirExists = SD.exists(slowGoalPath)) {
            // Get a random slow goal file path
            getSoundFilePath(slowGoalFilePath, slowGoalPath, slowGoalSoundIndices[random(0, slowGoalSoundCount-1)]);
        }
    }

    Serial.print(F("goal sound path is: "));
    Serial.println(goalFilePath);

    musicPlayer.startPlayingFile(goalFilePath);
}

void getSoundFilePath(char *soundFilePath, char *dirName, uint16_t themeFilePosition) {
    File themeFile = SD.open(themeFilePath, FILE_READ);
    themeFile.seek(themeFilePosition);
    char fileName[NAMELEN];
    readword(fileName, themeFile, NAMELEN - 1);
    strncpy(soundFilePath, dirName, GOALFILEPATHLEN - 1);
    strncat(soundFilePath, fileName, GOALFILEPATHLEN - strlen(soundFilePath) - 1);
}

void readTheme(char *dirName) {
    strncpy(themeFilePath, dirName, THEMEFILEPATHLEN - 1);
    strncat(themeFilePath, THEMEFILENAME, THEMEFILEPATHLEN - 1);
    File themeFile = SD.open(themeFilePath, FILE_READ);
    if (!themeFile.available()) {
        Serial.println(F("theme.txt not found"));
        return;
    }
    goalSoundCount = fastGoalSoundCount = slowGoalSoundCount = 0;
    char fileLine[FILELINELEN];
    uint16_t **currentlyReading;
    uint8_t *currentlyCounting;
    uint16_t lineStartPosition;
    while (themeFile.available()) {
        lineStartPosition = themeFile.position();
        readline(fileLine, themeFile, FILELINELEN - 1);
        if (strncmp(fileLine, "#", 1) == 0) {
            continue;
        } else if (strncmp(fileLine, GOALDIR, 5) == 0 && strlen(fileLine) == 5) {
            currentlyReading = &goalSoundIndices;
            currentlyCounting = &goalSoundCount;
        } else if (strncmp(fileLine, FASTGOALDIR, 9) == 0) {
            currentlyReading = &fastGoalSoundIndices;
            currentlyCounting = &fastGoalSoundCount;
        } else if (strncmp(fileLine, SLOWGOALDIR, 9) == 0) {
            currentlyReading = &slowGoalSoundIndices;
            currentlyCounting = &slowGoalSoundCount;
        } else {
            uint16_t *tmp = (uint16_t *) realloc(*currentlyReading, (*currentlyCounting+1) * sizeof(uint16_t *));
            if (*tmp == NULL) {
                Serial.println(F("ran out of memory during realloc()"));
                break;
            }
            *currentlyReading = tmp;
            (*currentlyReading)[*currentlyCounting] = lineStartPosition;
/*
            Serial.print(*currentlyCounting); Serial.print(F(" "));
            Serial.println((*currentlyReading)[*currentlyCounting]);
*/
            (*currentlyCounting)++;
        }
    }
    themeFile.close();
/*
    Serial.println(F("&&"));
    Serial.print(F("goalSoundIndices: "));
    for (int j=0; j < 20; j++) {
        Serial.print(goalSoundIndices[j]); Serial.print(F(" "));
    }
    Serial.println(F("@@"));
    Serial.println(goalSoundCount);
*/
}

void readline(char *line, File file, uint8_t maxlen) {
    uint8_t i = 0;
    char c;
    while (i < maxlen && file.available()) {
        c = (char)file.read();
        if (c == '\r' || c == '\n') {
            break;
        }
        line[i] = c;
        i++;
    }
    line[i] = '\0';
}

void readword(char *word, File file, uint8_t maxlen) {
    uint8_t i = 0;
    char c;
    while (i < maxlen && file.available()) {
        c = (char)file.read();
        if (c == ' ' || c == ',' || c == '\t' || c == '\r' || c == '\n') {
            break;
        }
        word[i] = c;
        i++;
    }
    word[i] = '\0';
}

char ** listFiles(char *dirName, uint8_t *numFiles) {
    uint8_t i = 0;
    //char filename[NAMELEN];
    //char **fileList = NULL;
    //Serial.print(F("goal sound now is: "));
    //Serial.println(goalSounds[7]);

    File dir = SD.open(dirName);
    File entry = dir.openNextFile();
    char **fileList = NULL;
/*
    char **fileList = (char **) malloc(sizeof(char *[1]));
    if (*fileList == NULL) {
        Serial.println(F("ran out of memory during malloc()"));
        return fileList;
    }
*/
    while (entry) {
        Serial.print(F("file found: "));
        Serial.println(entry.name());
/*
        if (i == 0) {
            // Malloc space for the first entry pointer
            fileList = (char **) malloc(sizeof(char *));
            if (*fileList == NULL) {
                Serial.println(F("ran out of memory during malloc()"));
                break;
            }
        } else {
*/
        //if (i != 0) {
            // Realloc space for remaining entry pointers
            char **tmp = (char **) realloc(fileList, sizeof(char *[i+1]));
            if (*tmp == NULL) {
                Serial.println(F("ran out of memory during realloc()"));
                break;
            }
            fileList = tmp;
        //}
        //strncpy(filename, entry.name(), NAMELEN-1);
        // Malloc space for the filename entry itself in the array
        fileList[i] = (char *) malloc(strlen(entry.name())+1);
        if (fileList[i] == NULL) {
            Serial.println(F("ran out of memory during filename malloc()"));
            break;
        }
        strncpy(fileList[i], entry.name(), strlen(entry.name())+1);
        entry.close();
        i++;
        entry = dir.openNextFile();
    }
    entry.close();
    dir.close();
    *numFiles = i;
    return fileList;
}
