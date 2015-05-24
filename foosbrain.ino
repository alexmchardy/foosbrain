/* 
  IR Breakbeam with Mp3
*/

// include SPI, MP3 and SdFat libraries
#include <SPI.h>
#include <Adafruit_VS1053.h>
#include <SdFat.h>
// SD file system object
SdFat SD;

// These are the pins used for the music maker shield
#define SHIELD_RESET  -1      // VS1053 reset pin (unused!)
#define SHIELD_CS     7      // VS1053 chip select pin (output)
#define SHIELD_DCS    6      // VS1053 Data/command select pin (output)
// These are common pins between breakout and shield
#define CARDCS 4     // Card chip select pin
// DREQ should be an Int pin, see http://arduino.cc/en/Reference/attachInterrupt
#define DREQ 3       // VS1053 Data request, ideally an Interrupt pin
// Create shield-example object
Adafruit_VS1053_FilePlayer musicPlayer =
    Adafruit_VS1053_FilePlayer(SHIELD_RESET, SHIELD_CS, SHIELD_DCS, DREQ, CARDCS);

// Unused analog pin for seeding random number generator
#define UNUSED_ANALOG_PIN A0

// Pin 13: Arduino has an LED connected on pin 13
#define LEDPIN 13

// Goal detection
#define GOAL_PIN_BLACK 20
#define GOAL_INTERRUPT_NUM_BLACK 3
#define GOAL_PIN_YELLOW 21
#define GOAL_INTERRUPT_NUM_YELLOW 2

// Theme change button
#define THEME_BUTTON_PIN 2
#define THEME_BUTTON_INTERRUPT_NUM 0

// File reading/writing
const uint8_t NAMELEN = 13;
const uint8_t FILELINELEN = 35;
const uint8_t SETTINGSPATHLEN = 18;
const uint8_t THEMEPATHLEN = 40; // 3*12 + 3 ("/"s) + 1 ("\0")
const uint8_t GOALPATHLEN = 53; // 4*12 + 4 ("/"s) + 1 ("\0")
const uint8_t THEMEFILEPATHLEN = 53; // 4*12 + 4 ("/"s) + 1 ("\0")
const uint8_t GOALFILEPATHLEN = 66; // 5*12 + 5 ("/"s) + 1 ("\0")
const uint8_t QUEUESIZE = 3;
const char DIRSEP[2] PROGMEM = "/";
const char ROOT[6] PROGMEM = "foos/";
const char SETTINGSFILENAME[13] PROGMEM = "settings.txt";
const char THEMEDIRKEY[10] PROGMEM = "themeDir:";
const char GOALSOUNDDELAYKEY[16] PROGMEM = "goalSoundDelay:";
const char THEMESDIR[8] PROGMEM = "themes/";
const char THEMEFILENAME[11] PROGMEM = "/theme.txt";
const char KEYSOUNDKEY[10] PROGMEM = "keySound:";
const char GOALDIR[7] PROGMEM = "/goal/";
const char FASTGOALDIR[11] PROGMEM = "/goalfast/";
const char SLOWGOALDIR[11] PROGMEM = "/goalslow/";
bool fastGoalDirExists = false;
bool slowGoalDirExists = false;
char settingsFilePath[SETTINGSPATHLEN];
uint16_t *themeIndices = NULL;
uint8_t themeCount = 0;
uint8_t currentThemeIndex = 0;
char themeDirName[NAMELEN];
char themeFilePath[THEMEFILEPATHLEN];
char themePath[THEMEPATHLEN];
char themeKeySoundFilePath[GOALFILEPATHLEN];
char goalPath[GOALPATHLEN];
char fastGoalPath[GOALPATHLEN];
char slowGoalPath[GOALPATHLEN];
char goalFilePath[GOALFILEPATHLEN];
char goalFilePathQueue[QUEUESIZE][GOALFILEPATHLEN];
char fastGoalFilePath[GOALFILEPATHLEN];
char slowGoalFilePath[GOALFILEPATHLEN];
uint8_t queueHead = 0;
uint8_t goalsQueued = 0;
uint32_t goalSoundDelay = 0;
uint16_t *goalSoundIndices = NULL;
uint16_t *fastGoalSoundIndices = NULL;
uint16_t *slowGoalSoundIndices = NULL;
uint8_t goalSoundCount;
uint8_t fastGoalSoundCount;
uint8_t slowGoalSoundCount;
ifstream themeFileStream;

// Setup theme button detection
#define THEME_BUTTON_DEBOUNCE_MS 300
volatile bool themeButtonPressed = false;
uint32_t themeButtonPressTime = 0;

// Setup goal detection
#define GOAL_DEBOUNCE_MS 100
#define GOAL_MS_SLOW 20
#define GOAL_MS_FAST 10
uint32_t blackGoalStartTime = 0;
volatile uint32_t blackGoalTime = 0;
uint32_t yellowGoalStartTime = 0;
volatile uint32_t yellowGoalTime = 0;

void blackGoalSensorChange() {
    uint32_t time = millis();
    uint8_t state = digitalRead(GOAL_PIN_BLACK);
    if (state == LOW) {
        if ((time - blackGoalStartTime) > GOAL_DEBOUNCE_MS) {
            blackGoalStartTime = time;
        }
    } else {
        if (blackGoalStartTime > 0) {
            blackGoalTime = millis() - blackGoalStartTime;
            blackGoalStartTime = 0;
        }
    }
}

void yellowGoalSensorChange() {
    uint32_t time = millis();
    uint8_t state = digitalRead(GOAL_PIN_YELLOW);
    if (state == LOW) {
        if ((time - yellowGoalStartTime) > GOAL_DEBOUNCE_MS) {
            yellowGoalStartTime = time;
        }
    } else {
        if (yellowGoalStartTime > 0) {
            yellowGoalTime = millis() - yellowGoalStartTime;
            yellowGoalStartTime = 0;
        }
    }
}

/**
 * Detects debounced theme button press, setting themeButtonPressed to true
 */
void themeButtonFalling() {
    uint32_t time = millis();
    if ((time - themeButtonPressTime) > THEME_BUTTON_DEBOUNCE_MS) {
        themeButtonPressed = true;
        themeButtonPressTime = time;
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

    // Make a tone to indicate we're ready to roll
    musicPlayer.sineTest(0x44, 500);

    // Initialize the SD card
    initSoundFiles();

    // initialize the LED pin as an output:
    pinMode(LEDPIN, OUTPUT);

    // Set up theme-change button
    pinMode(THEME_BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(THEME_BUTTON_INTERRUPT_NUM, themeButtonFalling, FALLING);

    // Set the goal sensors pins
    pinMode(GOAL_PIN_BLACK, INPUT_PULLUP);
    pinMode(GOAL_PIN_YELLOW, INPUT_PULLUP);

    // Attach goal sensor interrupts
    attachInterrupt(GOAL_INTERRUPT_NUM_BLACK, blackGoalSensorChange, CHANGE);
    attachInterrupt(GOAL_INTERRUPT_NUM_YELLOW, yellowGoalSensorChange, CHANGE);
}

void loop(){
    char soundToPlay[GOALFILEPATHLEN];
    if (blackGoalTime > 0 || yellowGoalTime > 0) {
        if (blackGoalTime) {
            musicPlayer.stopPlaying();
            musicPlayer.setVolume(20,254);
            getGoalSound(soundToPlay, blackGoalTime);
            blackGoalStartTime = 0;
            blackGoalTime = 0;
            Serial.print(F("Black Goal: ")); Serial.print(blackGoalTime); Serial.println(F(" ms"));
            if (strncmp(soundToPlay, "\0", 1) == 0) {
                Serial.println(F("Failed to find a sound file."));
                return;
            }
            Serial.print(F("playing: ")); Serial.println(soundToPlay);
            musicPlayer.startPlayingFile(soundToPlay);
        }
        if (yellowGoalTime) {
            musicPlayer.stopPlaying();
            musicPlayer.setVolume(254,20);
            getGoalSound(soundToPlay, yellowGoalTime);
            yellowGoalStartTime = 0;
            yellowGoalTime = 0;
            Serial.print(F("Yellow Goal: ")); Serial.print(yellowGoalTime); Serial.println(F(" ms"));
            if (strncmp(soundToPlay, "\0", 1) == 0) {
                Serial.println(F("Failed to find a sound file."));
                return;
            }
            Serial.print(F("playing: ")); Serial.println(soundToPlay);
            musicPlayer.startPlayingFile(soundToPlay);
        }
    } else if (themeButtonPressed) {
        themeButtonPressed = false;
        setNextTheme();
    } else if (goalsQueued < QUEUESIZE) {
        fillSoundFilePathQueue(goalPath);
    }
}

/**
 * Sets first param to a goal sound path based on the goal time
 *
 */
void getGoalSound(char *goalSound, uint32_t goalTime) {
    uint8_t i = 0;
    char (*tmp)[GOALFILEPATHLEN];
/*
    if (fastGoalDirExists && goalTime < 10) {
        strncpy(goalSound, fastGoalFilePath, GOALFILEPATHLEN - 1);
    } else if (slowGoalDirExists && goalTime > 30) {
        strncpy(goalSound, slowGoalFilePath, GOALFILEPATHLEN - 1);
    } else {
    }
*/
    // Attempt to refill the queue first if it's empty
    if (goalsQueued == 0) {
        fillSoundFilePathQueue(goalPath);
        // Return null if we failed to refill the queue
        if (goalsQueued == 0) {
            strcpy(goalSound, "\0");
            return;
        }
    }
    // Pop a filename off the queue
    strncpy(goalSound, goalFilePathQueue[queueHead], GOALFILEPATHLEN - 1);
    queueHead = (queueHead + 1) % QUEUESIZE;
    goalsQueued--;
}

/*
void queueNewGoalSound(uint32_t goalTime) {
    if (fastGoalDirExists && goalTime < 10) {
        getSoundFilePath(fastGoalFilePath, fastGoalPath, fastGoalSoundIndices[random(0, fastGoalSoundCount-1)]);
    } else if (slowGoalDirExists && goalTime > 30) {
        getSoundFilePath(slowGoalFilePath, slowGoalPath, slowGoalSoundIndices[random(0, slowGoalSoundCount-1)]);
    } else {
        getSoundFilePath(goalFilePath, goalPath, goalSoundIndices[random(0, goalSoundCount-1)]);
    }
}
*/

void setNextTheme() {
    // Build theme path
    strncpy_P(themePath, ROOT, THEMEPATHLEN-1);
    strncat_P(themePath, THEMESDIR, THEMEPATHLEN - strlen(themePath) - 1);
    getNextTheme(themePath);
    Serial.print(F("Got next theme: ")); Serial.println(themePath);
    if (strncmp(themePath, "\0", 1) == 0) {
        return;
    }
    setTheme(themePath);
    Serial.print(F("Set next theme: ")); Serial.println(themePath);
    //TODO: saveThemeInSettings();
}

inline void initSoundFiles() {
    if (!SD.begin(CARDCS)) {
        SD.initErrorHalt();
    }

    // Read settings file
    readSettings();

    // Build theme path from theme dir in settings file
    strncpy_P(themePath, ROOT, THEMEPATHLEN-1);
    strncat_P(themePath, THEMESDIR, THEMEPATHLEN - strlen(themePath) - 1);
    strncat(themePath, themeDirName, THEMEPATHLEN - strlen(themePath) - 1);

    // Set the theme
    setTheme(themePath);
}

inline void readSettings() {
    strncpy_P(settingsFilePath, ROOT, SETTINGSPATHLEN-1);
    if (!SD.exists(settingsFilePath)) {
        Serial.println(F("Error: foos/ dir not found on SD card"));
    }
    strncat_P(settingsFilePath, SETTINGSFILENAME, SETTINGSPATHLEN - strlen(settingsFilePath) - 1);
    if (!SD.exists(settingsFilePath)) {
        Serial.println(F("Error: foos/settings.txt not found"));
    }
    ifstream settingsFileStream(settingsFilePath);
    if (!settingsFileStream.is_open()) {
        Serial.println(F("settings.txt not found"));
    }
    uint16_t lineStartPosition;
    bool readingThemes = false;
    char fileLine[FILELINELEN];
    while (1) {
        lineStartPosition = settingsFileStream.tellg();
        settingsFileStream.getline(fileLine, FILELINELEN);
        if (settingsFileStream.fail()) {
            break;
        }
        settingsFileStream.skipWhite();
        Serial.println(fileLine);
        if (strncmp(fileLine, "#", 1) == 0) {
            continue;
        } else if (strncmp_P(fileLine, THEMEDIRKEY, 9) == 0) {
            strncpy(themeDirName, fileLine+9, NAMELEN-1);
        } else if (strncmp_P(fileLine, GOALSOUNDDELAYKEY, 15) == 0) {
            char numStr[6];
            goalSoundDelay = atoi(strncpy(numStr, fileLine+15, 5));
        } else if (strncmp_P(fileLine, THEMESDIR, 6) == 0) {
            readingThemes = true;
        } else if (readingThemes) {
            uint16_t *tmp = (uint16_t *) realloc(themeIndices, (themeCount+1) * sizeof(uint16_t *));
            if (tmp == NULL) {
                Serial.println(F("ran out of memory during themeIndices realloc()"));
                break;
            }
            themeIndices = tmp;
            themeIndices[themeCount] = lineStartPosition;
            themeCount++;
        }
    }
    settingsFileStream.close();
}

// TODO: Seems to play static when new theme rotation gets around to first theme
void setTheme(char *themePathPassed) {

    // Read the theme.txt file
    readTheme(themePathPassed);

    // Build regular goal path
    strncpy(goalPath, themePathPassed, GOALPATHLEN-1);
    strncat_P(goalPath, GOALDIR, GOALPATHLEN - strlen(goalPath) - 1);
    if (goalSoundCount == 0 || !SD.exists(goalPath)) {
        Serial.print(F("goal path not found: ")); Serial.println(goalPath);
        return;
    }
    // Queue up a random regular goal file path
    randomSeed(analogRead(UNUSED_ANALOG_PIN));
    fillSoundFilePathQueue(goalPath);
    //getSoundFilePath(goalFilePath, goalPath, goalSoundIndices[random(0, goalSoundCount-1)]);

    // Build fast goal path
/*
    if (fastGoalSoundCount > 0) {
        strncpy(fastGoalPath, themePathPassed, GOALPATHLEN-1);
        strncat_P(fastGoalPath, FASTGOALDIR, GOALPATHLEN - strlen(fastGoalPath) - 1);
        if (fastGoalDirExists = SD.exists(fastGoalPath)) {
            // Queue up a random fast goal file path
            getSoundFilePath(fastGoalFilePath, fastGoalPath, fastGoalSoundIndices[random(0, fastGoalSoundCount-1)]);
        }
    }

    // Build slow goal path
    if (slowGoalSoundCount > 0) {
        strncpy(slowGoalPath, themePathPassed, GOALPATHLEN-1);
        strncat_P(slowGoalPath, SLOWGOALDIR, GOALPATHLEN - strlen(slowGoalPath) - 1);
        if (slowGoalDirExists = SD.exists(slowGoalPath)) {
            // Queue up a random slow goal file path
            getSoundFilePath(slowGoalFilePath, slowGoalPath, slowGoalSoundIndices[random(0, slowGoalSoundCount-1)]);
        }
    }
*/

    // Play the key sound for this theme if it exists
    if (SD.exists(themeKeySoundFilePath)) {
        musicPlayer.stopPlaying();
        Serial.print(F("Playing theme sound: ")); Serial.println(themeKeySoundFilePath);
        musicPlayer.startPlayingFile(themeKeySoundFilePath);
    }
}

void fillSoundFilePathQueue(char *dirName) {
    uint8_t queueFillPos = queueHead;
    // Walk down the queue starting below the queueHead, filling until full
    while (goalsQueued < QUEUESIZE) {
        queueFillPos = (queueFillPos == 0 ? QUEUESIZE : queueFillPos) - 1;
        getSoundFilePath(goalFilePathQueue[queueFillPos], dirName, goalSoundIndices[random(0, goalSoundCount-1)]);
        if (strncmp(goalFilePathQueue[queueFillPos], "\0", 1) == 0) {
            // Failed to get a soundFilePath, give up filling queue for now
            break;
        }
        goalsQueued++;
    }
}

/**
 * Gets sound path from the specified position in the theme file
 *
 * char *soundFilePath string into which sound path is stored
 * char *dirName sound file path to which filename is concatenated
 * uint16_t themeFilePosition byte position in theme file where sound filename can be found
 */
void getSoundFilePath(char *soundFilePath, char *dirName, uint16_t themeFilePosition) {
    strcpy(soundFilePath, "\0");
    if (!themeFileStream.is_open()) {
        Serial.println(F("Theme file stream lost"));
        return;
    }
    themeFileStream.seekg(themeFilePosition);
    char fileName[NAMELEN];
    if (musicPlayer.playingMusic) {
        Serial.println(F("Stopping playback to read theme file"));
        musicPlayer.stopPlaying();
    }
    themeFileStream.get(fileName, NAMELEN);
    strncpy(soundFilePath, dirName, GOALFILEPATHLEN - 1);
    strncat(soundFilePath, fileName, GOALFILEPATHLEN - strlen(soundFilePath) - 1);
}

void readTheme(char *dirName) {
    strncpy(themeFilePath, dirName, THEMEFILEPATHLEN - 1);
    strncat_P(themeFilePath, THEMEFILENAME, THEMEFILEPATHLEN - 1);
    if (themeFileStream.is_open()) {
        themeFileStream.close();
    }
    themeFileStream.open(themeFilePath);
    if (!themeFileStream.is_open()) {
        Serial.print(F("theme.txt not found in ")); Serial.println(dirName);
        return;
    }
    goalSoundCount = fastGoalSoundCount = slowGoalSoundCount = 0;
    char fileLine[FILELINELEN];
    uint16_t **currentlyReading;
    uint8_t *currentlyCounting;
    uint16_t lineStartPosition;
    while (1) {
        lineStartPosition = themeFileStream.tellg();
        themeFileStream.getline(fileLine, FILELINELEN);
        if (themeFileStream.fail()) {
            break;
        }
        themeFileStream.skipWhite();
        if (strncmp(fileLine, "#", 1) == 0) {
            continue;
        } else if (strncmp_P(fileLine, KEYSOUNDKEY, 9) == 0) {
            strncpy(themeKeySoundFilePath, dirName, GOALFILEPATHLEN - 1);
            strncat(themeKeySoundFilePath, fileLine+9, GOALFILEPATHLEN - strlen(themeKeySoundFilePath) - 1);
        } else if (strncmp_P(fileLine, GOALDIR, 5) == 0 && strlen(fileLine) == 5) {
            currentlyReading = &goalSoundIndices;
            currentlyCounting = &goalSoundCount;
        } else if (strncmp_P(fileLine, FASTGOALDIR, 9) == 0) {
            currentlyReading = &fastGoalSoundIndices;
            currentlyCounting = &fastGoalSoundCount;
        } else if (strncmp_P(fileLine, SLOWGOALDIR, 9) == 0) {
            currentlyReading = &slowGoalSoundIndices;
            currentlyCounting = &slowGoalSoundCount;
        } else {
            uint16_t *tmp = (uint16_t *) realloc(*currentlyReading, (*currentlyCounting+1) * sizeof(uint16_t *));
            if (tmp == NULL) {
                Serial.println(F("ran out of memory during goal sound realloc()"));
                break;
            }
            *currentlyReading = tmp;
            (*currentlyReading)[*currentlyCounting] = lineStartPosition;
            (*currentlyCounting)++;
        }
    }
}

void getNextTheme(char *themePathPassed) {
    ifstream settingsFileStream(settingsFilePath);
    if (++currentThemeIndex >= themeCount) {
        currentThemeIndex = 0;
    }
    settingsFileStream.seekg(themeIndices[currentThemeIndex]);
    settingsFileStream.get(themeDirName, NAMELEN);
    settingsFileStream.close();
    strncat(themePathPassed, themeDirName, THEMEPATHLEN - strlen(themePathPassed) - 1);
}
