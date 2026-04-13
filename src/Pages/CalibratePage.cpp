#include <Arduino.h>
#include "Page.h"
#include "Screen.h"
#include "Inputs.h"
#include "Globals.h"
#include "Helpers.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void waitEncRelease() {
    uint32_t t = millis();
    while (digitalRead(PIN_ENC_SW) == LOW) {
        if (millis() - t > 700) break;
        delay(1);
    }
}

// Draw a small horizontal bar showing PWM position relative to lo/mid/hi.
void CalibratePage::drawLiveBar(int x, int y, int w, int h,
                                uint16_t pwm,
                                uint16_t lo, uint16_t mid, uint16_t hi) {
    u8g2.drawFrame(x, y, w, h);
    // neutral tick at mid
    int midX = x + map((int)mid, (int)lo, (int)hi, 0, w - 1);
    u8g2.drawVLine(midX, y, h);
    // fill from neutral toward current value
    int curX = x + map(constrain((int)pwm, (int)lo, (int)hi), (int)lo, (int)hi, 0, w - 1);
    if (curX > midX)
        u8g2.drawBox(midX, y + 1, curX - midX, h - 2);
    else if (curX < midX)
        u8g2.drawBox(curX + 1, y + 1, midX - curX, h - 2);
}

// ---------------------------------------------------------------------------
// init
// ---------------------------------------------------------------------------
void CalibratePage::init() {
    state       = CAL_MENU;
    menuHovered = 0;
    encReady    = false;
    aReady      = false;
    bReady      = true;
    cReady      = true;
    calTestActive = false;
    getRotaryEncoderSpins(); // flush
}

// ---------------------------------------------------------------------------
// Menu sub-loop  (Throttle / Steer / Test)
// ---------------------------------------------------------------------------
void CalibratePage::loopMenu() {
    static const char* opts[] = { "Throttle", "Steer", "Test" };
    const int N = 3;

    int spins = getRotaryEncoderSpins();
    if (spins > 0) menuHovered = (menuHovered + 1) % N;
    if (spins < 0) menuHovered = (menuHovered - 1 + N) % N;

    drawPageHeader("< Home < Menu < ", "Calibrate");

    u8g2.setFont(FONT_TEXT);
    int baseY = 28;
    int rowH  = 14;
    for (int i = 0; i < N; i++) {
        int y = baseY + i * rowH;
        u8g2.drawStr(14, y, opts[i]);
        if (menuHovered == i) {
            int tw = u8g2.getStrWidth(opts[i]);
            u8g2.drawRFrame(10, y - 10, tw + 8, 13, 4);
        }
    }

    int encSw = getRotaryEncoderSwitchValue();
    if (encSw == UNPRESSED) encReady = true;
    if (encSw == PRESSED && encReady) {
        encReady = false;
        waitEncRelease();
        getRotaryEncoderSpins();
        if (menuHovered == 0) {
            tmpCtr = 0; tmpMin = 0; tmpMax = 0;
            state = CAL_THROTTLE_CTR;
        } else if (menuHovered == 1) {
            tmpCtr = 0; tmpMin = 0; tmpMax = 0;
            tmpSteerTrimPwm = (int16_t)steerCtrPwm; // start from saved value
            state = CAL_STEER_CTR;
        } else {
            state = CAL_TEST;
        }
    }
}

// ---------------------------------------------------------------------------
// Generic capture step — shows live ADC, press encoder to record
// ---------------------------------------------------------------------------
void CalibratePage::loopCapture(const char* axis, const char* step,
                                const char* hint,
                                CalibState  next,
                                uint16_t&   target) {
    int pin = (axis[0] == 'T') ? PIN_THROTTLE : PIN_STEER;
    int raw = analogRead(pin);

    drawPageHeader("< Calibrate < ", axis);

    u8g2.setFont(FONT_TEXT);
    u8g2.drawStr(4, 24, step);

    u8g2.setFont(FONT_TEXT_MONOSPACE);
    char buf[24];
    snprintf(buf, sizeof(buf), "Raw: %4d", raw);
    u8g2.drawStr(4, 38, buf);

    int barW = 120, barH = 5;
    int barX = 4, barY = 42;
    u8g2.drawFrame(barX, barY, barW, barH);
    int fill = map(raw, 0, 4095, 0, barW - 2);
    if (fill > 0) u8g2.drawBox(barX + 1, barY + 1, fill, barH - 2);

    u8g2.setFont(FONT_TINY_TEXT);
    u8g2.drawStr(4, 56, hint);
    u8g2.drawStr(4, 63, "Press ENC to record  A=cancel");

    int encSw = getRotaryEncoderSwitchValue();
    if (encSw == UNPRESSED) encReady = true;
    if (encSw == PRESSED && encReady) {
        encReady = false;
        target   = (uint16_t)raw;
        waitEncRelease();
        getRotaryEncoderSpins();
        state = next;
    }
}

// ---------------------------------------------------------------------------
// Steer step 2: live PWM trim — rotate encoder to nudge servo until straight
// ---------------------------------------------------------------------------
void CalibratePage::loopSteerCtrPwm() {
    // Encoder adjusts PWM 1 µs per click
    int spins = getRotaryEncoderSpins();
    tmpSteerTrimPwm = (int16_t)constrain((int)tmpSteerTrimPwm + spins, 500, 2500);

    // Broadcast test packet so the servo moves in real-time
    calTestActive    = true;
    calTestSteerPwm  = (uint16_t)tmpSteerTrimPwm;

    drawPageHeader("< Calibrate < ", "Steer");

    u8g2.setFont(FONT_TEXT);
    u8g2.drawStr(4, 20, "2. Trim center PWM");

    char buf[24];
    snprintf(buf, sizeof(buf), "PWM: %d us", tmpSteerTrimPwm);
    u8g2.setFont(FONT_TEXT_MONOSPACE);
    u8g2.drawStr(4, 32, buf);

    // Bar: show offset from the standard 1500 reference
    uint16_t barLo  = 1200, barHi = 1800;
    drawLiveBar(4, 34, 120, 5, (uint16_t)tmpSteerTrimPwm, barLo, 1500, barHi);

    // Offset readout
    int offset = (int)tmpSteerTrimPwm - 1500;
    snprintf(buf, sizeof(buf), "Offset: %+d us", offset);
    u8g2.setFont(FONT_TINY_TEXT);
    u8g2.drawStr(4, 48, buf);

    u8g2.drawStr(4, 56, "Rotate ENC: move servo");
    u8g2.drawStr(4, 63, "Wheels straight? Press ENC");

    int encSw = getRotaryEncoderSwitchValue();
    if (encSw == UNPRESSED) encReady = true;
    if (encSw == PRESSED && encReady) {
        encReady      = false;
        calTestActive = false;
        waitEncRelease();
        getRotaryEncoderSpins();
        state = CAL_STEER_MIN;
    }
}

// ---------------------------------------------------------------------------
// Steer step 5: direction check — sends full-right raw PWM, user confirms
// ---------------------------------------------------------------------------
void CalibratePage::loopSteerDir() {
    // Always send raw 2000 µs so the user can see which way the wheels go
    calTestActive   = true;
    calTestSteerPwm = 2000;

    if (getButtonValue(BTN_B) == UNPRESSED) bReady = true;
    if (getButtonValue(BTN_C) == UNPRESSED) cReady = true;

    drawPageHeader("< Calibrate < ", "Steer");

    u8g2.setFont(FONT_TEXT);
    u8g2.drawStr(4, 20, "4. Check direction");

    u8g2.setFont(FONT_TEXT_MONOSPACE);
    u8g2.drawStr(4, 33, "Servo at 2000us.");
    u8g2.drawStr(4, 43, "Wheels turn RIGHT?");

    // Right-pointing arrows
    u8g2.setFont(FONT_BOLD_HEADER);
    u8g2.drawStr(44, 55, "-->>");

    u8g2.setFont(FONT_TINY_TEXT);
    u8g2.drawStr(4, 63, "B=Yes(correct)  C=No(swap L/R)");

    if (getButtonValue(BTN_B) == PRESSED && bReady) {
        bReady        = false;
        calTestActive = false;
        invertSteer   = false;   // 2000 µs → right wheel movement, no invert needed
        state         = CAL_STEER_DONE;
    }
    if (getButtonValue(BTN_C) == PRESSED && cReady) {
        cReady        = false;
        calTestActive = false;
        invertSteer   = true;    // 2000 µs → left, so invert to make stick-right = wheel-right
        state         = CAL_STEER_DONE;
    }
}

// ---------------------------------------------------------------------------
// Done step — show captured values, save on ENC confirm
// ---------------------------------------------------------------------------
void CalibratePage::loopDone(const char* axis, CalibState nextAxis) {
    bool isThrottle = (axis[0] == 'T');

    drawPageHeader("< Calibrate < ", axis);

    u8g2.setFont(FONT_TEXT);
    u8g2.drawStr(4, 22, "Calibration done!");

    char buf[32];
    u8g2.setFont(FONT_TEXT_MONOSPACE);
    snprintf(buf, sizeof(buf), "Min: %4d", tmpMin);   u8g2.drawStr(4, 34, buf);
    snprintf(buf, sizeof(buf), "Ctr: %4d", tmpCtr);   u8g2.drawStr(4, 43, buf);
    snprintf(buf, sizeof(buf), "Max: %4d", tmpMax);   u8g2.drawStr(4, 52, buf);

    if (!isThrottle) {
        snprintf(buf, sizeof(buf), "Srv: %4d us", tmpSteerTrimPwm);
        u8g2.setFont(FONT_TINY_TEXT);
        u8g2.drawStr(68, 43, buf);
        const char* dirStr = invertSteer ? "Dir: INVERTED" : "Dir: NORMAL";
        u8g2.drawStr(68, 52, dirStr);
    }

    u8g2.setFont(FONT_TINY_TEXT);
    u8g2.drawStr(4, 63, "ENC=Save  A=Discard");

    int encSw = getRotaryEncoderSwitchValue();
    if (encSw == UNPRESSED) encReady = true;
    if (encSw == PRESSED && encReady) {
        encReady = false;
        if (tmpMin < tmpCtr && tmpCtr < tmpMax) {
            if (isThrottle) {
                throttleCalMin    = tmpMin;
                throttleCalCenter = tmpCtr;
                throttleCalMax    = tmpMax;
            } else {
                steerCalMin    = tmpMin;
                steerCalCenter = tmpCtr;
                steerCalMax    = tmpMax;
                steerCtrPwm    = (uint16_t)tmpSteerTrimPwm;
                saveProfiles(); // also persists invertSteer set by direction check
            }
            saveProfiles();
        }
        waitEncRelease();
        getRotaryEncoderSpins();
        state    = CAL_MENU;
        encReady = false;
    }
}

// ---------------------------------------------------------------------------
// Test / live readout view
// ---------------------------------------------------------------------------
void CalibratePage::loopTest() {
    uint16_t tRaw = (uint16_t)analogRead(PIN_THROTTLE);
    uint16_t sRaw = (uint16_t)analogRead(PIN_STEER);
    uint16_t tPwm = getThrottlePWM();
    uint16_t sPwm = getSteerPWM();

    DriveMode& m = driveModes[currentMode];

    drawPageHeader("< Calibrate < ", "Test");

    u8g2.setFont(FONT_TEXT_MONOSPACE);
    char buf[32];

    // Throttle row
    snprintf(buf, sizeof(buf), "T raw:%4d  %4d", tRaw, tPwm);
    u8g2.drawStr(4, 22, buf);
    drawLiveBar(4, 24, 120, 5, tPwm, m.minPWM, m.midPWM, m.maxPWM);

    // Steer row — use trimmed center for bar so it looks correct
    int trim    = (int)steerCtrPwm - 1500;
    uint16_t sLo  = (uint16_t)constrain(1000 + trim, 500, 2500);
    uint16_t sHi  = (uint16_t)constrain(2000 + trim, 500, 2500);
    snprintf(buf, sizeof(buf), "S raw:%4d  %4d", sRaw, sPwm);
    u8g2.drawStr(4, 37, buf);
    drawLiveBar(4, 39, 120, 5, sPwm, sLo, steerCtrPwm, sHi);

    u8g2.setFont(FONT_TINY_TEXT);
    snprintf(buf, sizeof(buf), "Mode:%s  T:%d-%d-%d",
             driveModes[currentMode].name,
             m.minPWM, m.midPWM, m.maxPWM);
    u8g2.drawStr(4, 53, buf);
    snprintf(buf, sizeof(buf), "Srv ctr:%d %s",
             steerCtrPwm, invertSteer ? "INV" : "NRM");
    u8g2.drawStr(4, 61, buf);
}

// ---------------------------------------------------------------------------
// Main loop dispatcher
// ---------------------------------------------------------------------------
void CalibratePage::loop() {
    // A button always cancels and goes back
    if (getButtonValue(BTN_A) == UNPRESSED) aReady = true;
    if (getButtonValue(BTN_A) == PRESSED && aReady) {
        aReady        = false;
        calTestActive = false;   // stop any test transmission
        if (state == CAL_MENU) {
            currentPage = menuPage;
            return;
        }
        state    = CAL_MENU;
        encReady = false;
        getRotaryEncoderSpins();
        return;
    }

    u8g2.clearBuffer();

    switch (state) {

        // ---- MENU ----
        case CAL_MENU:
            loopMenu();
            break;

        // ---- THROTTLE WIZARD ----
        case CAL_THROTTLE_CTR:
            loopCapture("Throttle", "1. Release throttle",
                        "Let stick rest at center",
                        CAL_THROTTLE_MIN, tmpCtr);
            break;
        case CAL_THROTTLE_MIN:
            loopCapture("Throttle", "2. Full reverse / min",
                        "Push stick to min position",
                        CAL_THROTTLE_MAX, tmpMin);
            break;
        case CAL_THROTTLE_MAX:
            loopCapture("Throttle", "3. Full forward / max",
                        "Push stick to max position",
                        CAL_THROTTLE_DONE, tmpMax);
            break;
        case CAL_THROTTLE_DONE:
            loopDone("Throttle", CAL_MENU);
            break;

        // ---- STEER WIZARD ----
        case CAL_STEER_CTR:
            loopCapture("Steer", "1. Center joystick",
                        "Release stick to resting position",
                        CAL_STEER_CTR_PWM, tmpCtr);
            break;
        case CAL_STEER_CTR_PWM:
            loopSteerCtrPwm();
            break;
        case CAL_STEER_MIN:
            loopCapture("Steer", "3. Full left / min",
                        "Push stick fully left",
                        CAL_STEER_MAX, tmpMin);
            break;
        case CAL_STEER_MAX:
            loopCapture("Steer", "4. Full right / max",
                        "Push stick fully right",
                        CAL_STEER_DIR, tmpMax);
            break;
        case CAL_STEER_DIR:
            loopSteerDir();
            break;
        case CAL_STEER_DONE:
            loopDone("Steer", CAL_MENU);
            break;

        // ---- TEST VIEW ----
        case CAL_TEST:
            loopTest();
            break;
    }

    u8g2.sendBuffer();
}