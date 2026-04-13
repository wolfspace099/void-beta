#include <Arduino.h>
#include "Page.h"
#include "Screen.h"
#include "Inputs.h"
#include "Globals.h"
#include "Helpers.h"

static String inputName = "";
static String inputDescription = "";
static int prevThrottleRaw = 0;
static int prevSteerRaw = 0;

void ControlsPage::init() {
    rotaryEncoderButtonReady = false;
    rotaryEncoderSwitchValue = UNPRESSED;
    inputName = "";
    inputDescription = "";
    prevThrottleRaw = analogRead(PIN_THROTTLE);
    prevSteerRaw = analogRead(PIN_STEER);
    getRotaryEncoderSpins();
}

void ControlsPage::loop() {
    static bool aReady = true;
    if (getButtonValue(BTN_A) == UNPRESSED) aReady = true;
    if (getButtonValue(BTN_A) == PRESSED && aReady) {
        aReady = false;
        currentPage = menuPage;
        return;
    }

    rotaryEncoderSwitchValue = getRotaryEncoderSwitchValue();
    if (rotaryEncoderSwitchValue == UNPRESSED) rotaryEncoderButtonReady = true;
    drawPageHeader("< Home < Menu < ", "Controls");

    if (rotaryEncoderSwitchValue == PRESSED && rotaryEncoderButtonReady) {
        inputName = "Encoder Press";
        inputDescription = "Select current item / confirm changes";
    } else if (getButtonValue(BTN_A) == PRESSED ||
               getButtonValue(BTN_B) == PRESSED ||
               getButtonValue(BTN_C) == PRESSED ||
               getButtonValue(BTN_D) == PRESSED) {
        inputName = "Navigation Button";
        if (getButtonValue(BTN_A) == PRESSED) {
            inputDescription = "A: Back/cancel, return to previous page";
        } else if (getButtonValue(BTN_B) == PRESSED) {
            inputDescription = "B: Secondary action (page-dependent)";
        } else if (getButtonValue(BTN_C) == PRESSED) {
            inputDescription = "C: Decrease/previous/quick action";
        } else {
            inputDescription = "D: Increase/next/open menu";
        }
    } else if (getRotaryEncoderSpins() != 0) {
        inputName = "Encoder Rotate";
        inputDescription = "Navigate menu and adjust values";
    } else {
        int t = analogRead(PIN_THROTTLE);
        int s = analogRead(PIN_STEER);
        if (abs(t - prevThrottleRaw) > 40) {
            inputName = "Throttle Stick";
            inputDescription = "Sets motor throttle output";
            prevThrottleRaw = t;
        } else if (abs(s - prevSteerRaw) > 40) {
            inputName = "Steer Stick";
            inputDescription = "Sets steering output";
            prevSteerRaw = s;
        }
    }

    if (inputName.length() == 0) {
        u8g2.setFont(FONT_HEADER);
        u8g2.drawStr(20, 40, "Use any input...");
        return;
    }

    u8g2.setFont(FONT_BOLD_HEADER);
    int x = 64 - (u8g2.getStrWidth(inputName.c_str()) / 2);
    u8g2.drawStr(x, 25, inputName.c_str());

    u8g2.setFont(FONT_TEXT);
    drawWrappedStr(inputDescription.c_str(), 2, 37, 127, true, 1);
}
