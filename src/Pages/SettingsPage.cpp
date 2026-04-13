#include <Arduino.h>
#include "Page.h"
#include "Screen.h"
#include "Inputs.h"
#include "Globals.h"
#include "Popup.h"
#include "Helpers.h"

enum SettingType { BOOLEAN, INTEGER, ACTION };

struct Setting {
    const char* name;
    SettingType type;
    void*       value;
    long        minVal;
    long        maxVal;
};

static long s_sleepDelay;
static long s_deadzone;

static Setting settings[] = {
    { "Sleep Delay",     INTEGER, &s_sleepDelay, 500, 120000 },
    { "Deadzone",        INTEGER, &s_deadzone,   0, 300 },
    { "Invert Throttle", BOOLEAN, &invertThrottle, 0, 0 },
    { "Invert Steer",    BOOLEAN, &invertSteer,    0, 0 },
    { "Lights",          ACTION,  nullptr, 0, 0 },
    { "Fan Preset",      ACTION,  nullptr, 0, 0 },
    { "Reset All",       ACTION,  nullptr, 0, 0 },
};
static const int numSettings = sizeof(settings) / sizeof(settings[0]);

static void syncFromGlobals() {
    s_sleepDelay = (long)screenSleepDelayMs;
    s_deadzone = (long)axisDeadzone;
}

static void syncToGlobals() {
    screenSleepDelayMs = (uint32_t)constrain(s_sleepDelay, 500L, 120000L);
    axisDeadzone = (uint16_t)constrain(s_deadzone, 0L, 300L);
    saveProfiles();
}

void SettingsPage::init() {
    rotaryEncoderButtonReady = false;
    rotaryEncoderSwitchValue = UNPRESSED;
    syncFromGlobals();
}

void SettingsPage::loop() {
    static bool aReady = true;
    if (getButtonValue(BTN_A) == UNPRESSED) aReady = true;
    if (getButtonValue(BTN_A) == PRESSED && aReady) {
        aReady = false;
        currentPage = menuPage;
        return;
    }

    rotaryEncoderSwitchValue = getRotaryEncoderSwitchValue();
    if (rotaryEncoderSwitchValue == UNPRESSED) rotaryEncoderButtonReady = true;

    int spins = getRotaryEncoderSpins();
    if (spins > 0 && hovered < numSettings - 1) hovered++;
    if (spins < 0 && hovered > 0) hovered--;

    drawPageHeader("< Home < Menu < ", "Settings");

    int rowSpacing = 14;
    int listYStart = 24;
    int listLeftSpacing = 7;
    if (hovered >= 2) listYStart -= rowSpacing * (hovered - 2);

    char buffer[40];
    u8g2.setFont(FONT_TEXT);

    for (int i = 0; i < numSettings; i++) {
        const char* name = settings[i].name;
        void* valuePtr = settings[i].value;

        if (settings[i].type == INTEGER) {
            if (strcmp(name, "Sleep Delay") == 0) {
                snprintf(buffer, sizeof(buffer), "%s: %ld ms", name, *(long*)valuePtr);
            } else {
                snprintf(buffer, sizeof(buffer), "%s: %ld", name, *(long*)valuePtr);
            }
        } else if (settings[i].type == BOOLEAN) {
            snprintf(buffer, sizeof(buffer), "%s: %s", name, (*(bool*)valuePtr) ? "On" : "Off");
        } else {
            if (strcmp(name, "Lights") == 0) {
                snprintf(buffer, sizeof(buffer), "Lights: %s", (frontLightCmd || rearLightCmd) ? "On" : "Off");
            } else if (strcmp(name, "Fan Preset") == 0) {
                snprintf(buffer, sizeof(buffer), "Fan: %u%%", (unsigned)fanPctCmd);
            } else {
                snprintf(buffer, sizeof(buffer), "%s", name);
            }
        }

        if (hovered < i + 3) {
            u8g2.drawStr(listLeftSpacing, listYStart, buffer);
            if (hovered == i) {
                u8g2.drawRFrame(listLeftSpacing - 4, listYStart - 10,
                                u8g2.getStrWidth(buffer) + 8, 13, 5);
            }
        }
        listYStart += rowSpacing;
    }

    if (rotaryEncoderSwitchValue == PRESSED && rotaryEncoderButtonReady) {
        rotaryEncoderButtonReady = false;

        if (settings[hovered].type == ACTION) {
            if (strcmp(settings[hovered].name, "Reset All") == 0) {
                String opts[] = { "Cancel", "RESET" };
                int choice = openPopupMultiChoice("Reset All?", opts, 2, 0);
                if (choice == 1) {
                    resetAll();
                    return;
                }
                return;
            }
            if (strcmp(settings[hovered].name, "Lights") == 0) {
                String opts[] = { "Off", "On" };
                int cur = (frontLightCmd || rearLightCmd) ? 1 : 0;
                int choice = openPopupMultiChoice("Lights", opts, 2, cur);
                if (choice < 0) return;
                frontLightCmd = (choice == 1) ? 1 : 0;
                rearLightCmd = frontLightCmd;
                saveProfiles();
                syncFromGlobals();
                return;
            }
            if (strcmp(settings[hovered].name, "Fan Preset") == 0) {
                String opts[] = { "0%", "25%", "50%", "75%", "100%" };
                int cur = 0;
                if (fanPctCmd >= 88) cur = 4;
                else if (fanPctCmd >= 63) cur = 3;
                else if (fanPctCmd >= 38) cur = 2;
                else if (fanPctCmd >= 13) cur = 1;
                int choice = openPopupMultiChoice("Fan", opts, 5, cur);
                if (choice < 0) return;
                static const uint8_t preset[] = {0, 25, 50, 75, 100};
                fanPctCmd = preset[choice];
                saveProfiles();
                syncFromGlobals();
                return;
            }
        }

        if (settings[hovered].type == BOOLEAN) {
            String opts[] = { "Off", "On" };
            int choice = openPopupMultiChoice(settings[hovered].name, opts, 2,
                                              *(bool*)settings[hovered].value ? 1 : 0);
            if (choice < 0) return;
            *(bool*)settings[hovered].value = (choice == 1);
            syncToGlobals();
        }

        if (settings[hovered].type == INTEGER) {
            long mn = settings[hovered].minVal;
            long mx = settings[hovered].maxVal;
            long val = *(long*)settings[hovered].value;
            long newVal = openPopupNumber(settings[hovered].name, constrain(val, mn, mx), mn, mx);
            *(long*)settings[hovered].value = newVal;
            syncToGlobals();
        }
    }

    drawScrollBar(numSettings, hovered);
}
