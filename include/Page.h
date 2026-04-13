#pragma once

#include <Arduino.h>
#include "Helpers.h"
#include "Globals.h"

class Page {
public:
    virtual void init() = 0;
    virtual void loop() = 0;
    virtual ~Page() = default;

    bool rotaryEncoderButtonReady = false;
    int  rotaryEncoderSwitchValue = UNPRESSED;
};

class HudPage : public Page {
public:
    void init() override;
    void loop() override;
private:
    long startTime = 0;
    long totalDrawTime = 0;
    bool pressInProgress = false;
    bool holdTriggered = false;
};

class MenuPage : public Page {
public:
    void init() override;
    void loop() override;
private:
    int hovered = 0;
    int numChoices = 4;
    bool backButtonReady = false;
};

class SettingsPage : public Page {
public:
    void init() override;
    void loop() override;
private:
    int hovered = 0;
};

class ControlsPage : public Page {
public:
    void init() override;
    void loop() override;
};

class ModesPage : public Page {
public:
    void init() override;
    void loop() override;
private:
    int hovered = 0;
};

class StatsPage : public Page {
public:
    void init() override;
    void loop() override;
private:
    int hovered = 0;
};

// Calibration wizard states
enum CalibState {
    CAL_MENU,
    CAL_THROTTLE_CTR,
    CAL_THROTTLE_MIN,
    CAL_THROTTLE_MAX,
    CAL_THROTTLE_DONE,
    CAL_STEER_CTR,
    CAL_STEER_CTR_PWM,  // trim servo center PWM with encoder
    CAL_STEER_MIN,
    CAL_STEER_MAX,
    CAL_STEER_DIR,      // direction check (B=correct, C=invert)
    CAL_STEER_DONE,
    CAL_TEST,
};

class CalibratePage : public Page {
public:
    void init() override;
    void loop() override;

    // Exposed so main.cpp can read them for live test packet sending
    bool     calTestActive  = false;
    uint16_t calTestSteerPwm = 1700;

private:
    CalibState state       = CAL_MENU;
    int        menuHovered = 0;
    bool       encReady    = false;
    bool       aReady      = false;
    bool       bReady      = true;
    bool       cReady      = true;

    // Temp cal values captured during wizard
    uint16_t tmpMin = 0, tmpCtr = 2048, tmpMax = 4095;
    int16_t  tmpSteerTrimPwm = 1700;

    void drawLiveBar(int x, int y, int w, int h, uint16_t pwm,
                     uint16_t lo, uint16_t mid, uint16_t hi);
    void loopMenu();
    void loopCapture(const char* axis, const char* step, const char* hint,
                     CalibState next, uint16_t& target);
    void loopSteerCtrPwm();
    void loopSteerDir();
    void loopDone(const char* axis, CalibState nextAxis);
    void loopTest();
};
