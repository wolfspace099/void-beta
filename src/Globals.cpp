#include <Arduino.h>
#include "Globals.h"
#include "Page.h"

namespace {
constexpr uint8_t kDefaultMode = 1;
constexpr uint32_t kDefaultScreenSleepDelayMs = 2000;
constexpr uint16_t kDefaultAxisMin = 0;
constexpr uint16_t kDefaultAxisCenter = 2048;
constexpr uint16_t kDefaultAxisMax = 4095;
constexpr uint16_t kDefaultAxisDeadzone = 40;
}

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

RCPacket packet = {RC_PROTOCOL_VERSION, 0, 1500, 1500, 1, 0, 0, 0, 1};
TelemetryPacket telePacketRx = {0};
Telemetry tele = {0, 0, false, 0, 0, 0, 0, 0, 0, 0};

DriveMode driveModes[MODE_COUNT] = {
    {"ECO", 1300, 1700, 1500},
    {"Normal", 1200, 1800, 1500},
    {"Turbo", 1000, 2000, 1500},
};
uint8_t currentMode = kDefaultMode;
Preferences prefs;

bool    armed = false;
float   ctrlBattVolt = 0;
uint8_t ctrlBattPct = 0;
uint8_t hudTick = 0;
uint8_t frontLightCmd = 0;
uint8_t rearLightCmd = 0;
uint8_t fanPctCmd = 0;
bool    invertThrottle = false;
bool    invertSteer = false;
uint16_t axisDeadzone = kDefaultAxisDeadzone;
uint16_t throttleCalMin = kDefaultAxisMin;
uint16_t throttleCalCenter = kDefaultAxisCenter;
uint16_t throttleCalMax = kDefaultAxisMax;
uint16_t steerCalMin = kDefaultAxisMin;
uint16_t steerCalCenter = kDefaultAxisCenter;
uint16_t steerCalMax = kDefaultAxisMax;
uint16_t steerCtrPwm = 1700;
bool     mpuAvailable = false;
float    mpuHeadingDeg = 0.0f;
float    statsTopSpeedKmh = 0.0f;
uint32_t statsRunSeconds = 0;
uint32_t bootMillis = 0;
uint32_t screenSleepDelayMs = kDefaultScreenSleepDelayMs;
uint32_t lastInputActivityMs = 0;
bool     displaySleeping = false;

volatile int encRawCount = 0;

HudPage*      hudPage = new HudPage();
MenuPage*     menuPage = new MenuPage();
SettingsPage* settingsPage = new SettingsPage();
ControlsPage* controlsPage = new ControlsPage();
ModesPage*    modesPage = new ModesPage();
StatsPage*     statsPage     = new StatsPage();
CalibratePage* calibratePage = new CalibratePage();

Page* currentPage = nullptr;
Page* previousPage = nullptr;

void saveProfiles() {
    prefs.putUChar("mode", currentMode);
    prefs.putUChar("frontL", frontLightCmd);
    prefs.putUChar("rearL", rearLightCmd);
    prefs.putUChar("fanPct", fanPctCmd);
    prefs.putBool("invT", invertThrottle);
    prefs.putBool("invS", invertSteer);
    prefs.putUShort("deadz", axisDeadzone);
    prefs.putUShort("tMin", throttleCalMin);
    prefs.putUShort("tCtr", throttleCalCenter);
    prefs.putUShort("tMax", throttleCalMax);
    prefs.putUShort("sMin", steerCalMin);
    prefs.putUShort("sCtr", steerCalCenter);
    prefs.putUShort("sMax", steerCalMax);
    prefs.putUShort("sCtrPwm", steerCtrPwm);
    prefs.putFloat("topKmh", statsTopSpeedKmh);
    prefs.putULong("runSec", statsRunSeconds);
    prefs.putULong("scrSleep", screenSleepDelayMs);
}

void loadProfiles() {
    currentMode = prefs.getUChar("mode", kDefaultMode);
    frontLightCmd = prefs.getUChar("frontL", 0);
    rearLightCmd = prefs.getUChar("rearL", 0);
    fanPctCmd = prefs.getUChar("fanPct", 0);
    invertThrottle = prefs.getBool("invT", false);
    invertSteer = prefs.getBool("invS", false);
    axisDeadzone = prefs.getUShort("deadz", kDefaultAxisDeadzone);
    throttleCalMin = prefs.getUShort("tMin", kDefaultAxisMin);
    throttleCalCenter = prefs.getUShort("tCtr", kDefaultAxisCenter);
    throttleCalMax = prefs.getUShort("tMax", kDefaultAxisMax);
    steerCalMin = prefs.getUShort("sMin", kDefaultAxisMin);
    steerCalCenter = prefs.getUShort("sCtr", kDefaultAxisCenter);
    steerCalMax = prefs.getUShort("sMax", kDefaultAxisMax);
    steerCtrPwm = prefs.getUShort("sCtrPwm", 1700);
    statsTopSpeedKmh = prefs.getFloat("topKmh", 0.0f);
    statsRunSeconds = prefs.getULong("runSec", 0);
    screenSleepDelayMs = prefs.getULong("scrSleep", kDefaultScreenSleepDelayMs);
    axisDeadzone = constrain(axisDeadzone, 0U, 300U);
    screenSleepDelayMs = constrain(screenSleepDelayMs, 500UL, 120000UL);
}

void resetAll() {
    currentMode = kDefaultMode;
    frontLightCmd = 0;
    rearLightCmd = 0;
    fanPctCmd = 0;
    invertThrottle = false;
    invertSteer = false;
    axisDeadzone = kDefaultAxisDeadzone;
    throttleCalMin = kDefaultAxisMin;
    throttleCalCenter = kDefaultAxisCenter;
    throttleCalMax = kDefaultAxisMax;
    steerCalMin = kDefaultAxisMin;
    steerCalCenter = kDefaultAxisCenter;
    steerCalMax = kDefaultAxisMax;
    steerCtrPwm = 1700;
    statsTopSpeedKmh = 0.0f;
    statsRunSeconds = 0;
    screenSleepDelayMs = kDefaultScreenSleepDelayMs;
    saveProfiles();
    currentPage = hudPage;
}
