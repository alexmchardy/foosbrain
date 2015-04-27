/* 
  IR Breakbeam with Mp3
*/
// include PinChangeInt (with optimization switches)
#define NO_PORTB_PINCHANGES // to indicate that port b will not be used for pin change interrupts
#define NO_PORTJ_PINCHANGES // to indicate that port j will not be used for pin change interrupts
#include <PinChangeInt.h>

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
#define UNUSED_ANALOG_PIN 0

// Pin 13: Arduino has an LED connected on pin 13
#define LEDPIN 13

// Goal detection
#define GOAL_PIN_BLACK A8
#define GOAL_PIN_YELLOW A9

// Theme change button
#define THEME_BUTTON_PIN A10

// File reading/writing
const uint8_t NAMELEN = 13;
const uint8_t FILELINELEN = 35;
const uint8_t SETTINGSPATHLEN = 18;
const uint8_t THEMEPATHLEN = 40; // 3*12 + 3 ("/"s) + 1 ("\0")
const uint8_t GOALPATHLEN = 53; // 4*12 + 4 ("/"s) + 1 ("\0")
const uint8_t THEMEFILEPATHLEN = 53; // 4*12 + 4 ("/"s) + 1 ("\0")
const uint8_t GOALFILEPATHLEN = 66; // 5*12 + 5 ("/"s) + 1 ("\0")
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
char fastGoalFilePath[GOALFILEPATHLEN];
char slowGoalFilePath[GOALFILEPATHLEN];
uint32_t goalSoundDelay = 0;
uint16_t *goalSoundIndices = NULL;
uint16_t *fastGoalSoundIndices = NULL;
uint16_t *slowGoalSoundIndices = NULL;
uint8_t goalSoundCount;
uint8_t fastGoalSoundCount;
uint8_t slowGoalSoundCount;

volatile bool themeButtonPressed = false;
uint32_t themeButtonPressTime = 0;

// Setup goal detection
#define GOAL_MS_SLOW 20
#define GOAL_MS_FAST 10
uint32_t blackGoalStartTime = 0;
volatile uint32_t blackGoalTime = 0;
uint32_t yellowGoalStartTime = 0;
volatile uint32_t yellowGoalTime = 0;

void goalSensorChange() {
    uint32_t time = millis();
    uint32_t *startTime;
    volatile uint32_t *goalTime;
    if (PCintPort::arduinoPin == GOAL_PIN_BLACK) {
        startTime = &blackGoalStartTime;
        goalTime = &blackGoalTime;
    } else {
        startTime = &yellowGoalStartTime;
        goalTime = &yellowGoalTime;
    }
    if (PCintPort::pinState == LOW && (time - *startTime) > 100) {
        *startTime = time;
    }
    if (PCintPort::pinState == HIGH && *startTime > 0) {
        *goalTime = millis() - *startTime;
        *startTime = 0;
    }
}

void themeButtonChange() {
    uint32_t time = millis();
    if (PCintPort::arduinoPin == THEME_BUTTON_PIN && PCintPort::pinState == LOW && (time - themeButtonPressTime) > 500) {
        themeButtonPressed = true;
        themeButtonPressTime = millis();
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
    digitalWrite(THEME_BUTTON_PIN, INPUT_PULLUP);
    attachPinChangeInterrupt(THEME_BUTTON_PIN, themeButtonChange, CHANGE);

    // Set the goal sensors pins
    digitalWrite(GOAL_PIN_BLACK, INPUT_PULLUP);
    digitalWrite(GOAL_PIN_YELLOW, INPUT_PULLUP);

    // Attach goal sensor interrupts
    attachPinChangeInterrupt(GOAL_PIN_BLACK, goalSensorChange, CHANGE);
    attachPinChangeInterrupt(GOAL_PIN_YELLOW, goalSensorChange, CHANGE);
}

void loop(){
    char soundToPlay[GOALFILEPATHLEN];
    if (blackGoalTime > 0 || yellowGoalTime > 0) {
        if (blackGoalTime) {
            Serial.print(F("Black Goal: ")); Serial.print(blackGoalTime); Serial.println(F(" ms"));
            getGoalSound(soundToPlay, blackGoalTime);
            blackGoalStartTime = 0;
            blackGoalTime = 0;
            musicPlayer.setVolume(20,254);
            musicPlayer.stopPlaying();
            musicPlayer.startPlayingFile(soundToPlay);
            queueNewGoalSound(blackGoalTime);
        }
        if (yellowGoalTime) {
            Serial.print(F("Yellow Goal: ")); Serial.print(yellowGoalTime); Serial.println(F(" ms"));
            getGoalSound(soundToPlay, yellowGoalTime);
            yellowGoalTime = 0;
            musicPlayer.setVolume(254,20);
            musicPlayer.stopPlaying();
            musicPlayer.startPlayingFile(soundToPlay);
            queueNewGoalSound(yellowGoalTime);
        }
    }
    if (themeButtonPressed) {
        themeButtonPressed = false;
        setNextTheme();
    }
}

/**
 * Sets first param to a goal sound path based on the goal time
 *
 */
void getGoalSound(char *goalSound, uint32_t goalTime) {
    if (fastGoalDirExists && goalTime < 10) {
        strncpy(goalSound, fastGoalFilePath, GOALFILEPATHLEN - 1);
    } else if (slowGoalDirExists && goalTime > 30) {
        strncpy(goalSound, slowGoalFilePath, GOALFILEPATHLEN - 1);
    } else {
        strncpy(goalSound, goalFilePath, GOALFILEPATHLEN - 1);
    }
}

void queueNewGoalSound(uint32_t goalTime) {
    if (fastGoalDirExists && goalTime < 10) {
        getSoundFilePath(fastGoalFilePath, fastGoalPath, fastGoalSoundIndices[random(0, fastGoalSoundCount-1)]);
    } else if (slowGoalDirExists && goalTime > 30) {
        getSoundFilePath(slowGoalFilePath, slowGoalPath, slowGoalSoundIndices[random(0, slowGoalSoundCount-1)]);
    } else {
        getSoundFilePath(goalFilePath, goalPath, goalSoundIndices[random(0, goalSoundCount-1)]);
    }
}

void setNextTheme() {
    // Build theme path
    strncpy_P(themePath, ROOT, THEMEPATHLEN-1);
    strncat_P(themePath, THEMESDIR, THEMEPATHLEN - strlen(themePath) - 1);
    getNextTheme(themePath);
    setTheme(themePath);
    //saveThemeInSettings();
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
    getSoundFilePath(goalFilePath, goalPath, goalSoundIndices[random(0, goalSoundCount-1)]);

    // Build fast goal path
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

    // Play the key sound for this theme if it exists
    if (SD.exists(themeKeySoundFilePath)) {
        Serial.print(F("Playing: ")); Serial.println(themeKeySoundFilePath);
        musicPlayer.stopPlaying();
        musicPlayer.startPlayingFile(themeKeySoundFilePath);
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
    ifstream themeFileStream(themeFilePath);
    if (!themeFileStream.is_open()) {
        Serial.print(F("Unable to open theme.txt in ")); Serial.println(dirName);
    }
    themeFileStream.seekg(themeFilePosition);
    char fileName[NAMELEN];
    themeFileStream.get(fileName, NAMELEN);
    themeFileStream.close();
    strncpy(soundFilePath, dirName, GOALFILEPATHLEN - 1);
    strncat(soundFilePath, fileName, GOALFILEPATHLEN - strlen(soundFilePath) - 1);
}

void readTheme(char *dirName) {
    strncpy(themeFilePath, dirName, THEMEFILEPATHLEN - 1);
    strncat_P(themeFilePath, THEMEFILENAME, THEMEFILEPATHLEN - 1);
    ifstream themeFileStream(themeFilePath);
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
    themeFileStream.close();
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
/*
    uint16_t dirIndex;
    SdFile dir;
    SdFile entry;
    SdFile next;
    dir.open(themePathPassed, O_READ);
    entry.open(&dir, themeDirName, O_READ);
    entry.close();
    next.openNext(&dir, O_READ);
    next.getName(themeDirName, NAMELEN);
    Serial.print(F("Next theme: ")); Serial.println(themeDirName);
    next.close();
    if (!next.openNext(&dir, O_READ)) {
        dir.rewind();
        next.openNext(&dir, O_READ);
        next.getName(themeDirName, NAMELEN);
        Serial.print(F("Rewound next theme: ")); Serial.println(themeDirName);
    } else {
        next.getName(themeDirName, NAMELEN);
        Serial.print(F("Next next theme: ")); Serial.println(themeDirName);
    }
    strncat(themePathPassed, themeDirName, THEMEPATHLEN - strlen(themePathPassed) - 1);
    next.close();
    dir.close();
*/
}
