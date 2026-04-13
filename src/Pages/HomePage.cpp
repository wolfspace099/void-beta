#include <Arduino.h>
#include <math.h>
#include "Page.h"
#include "Screen.h"
#include "Inputs.h"
#include "Globals.h"
#include "Icons.h"
#include "Helpers.h"
#include "Popup.h"

void HudPage::init() {
    rotaryEncoderButtonReady = false;
    rotaryEncoderSwitchValue = UNPRESSED;
    bool btnDownNow = (digitalRead(PIN_ENC_SW) == LOW);
    pressInProgress = btnDownNow;
    holdTriggered = btnDownNow;
}

void HudPage::loop() {
    startTime = millis();

    static bool bReady = true;
    static bool cReady = true;
    static bool dReady = true;

    if (getButtonValue(BTN_B) == UNPRESSED) bReady = true;
    if (getButtonValue(BTN_C) == UNPRESSED) cReady = true;
    if (getButtonValue(BTN_D) == UNPRESSED) dReady = true;

    if (getButtonValue(BTN_D) == PRESSED && dReady) {
        dReady = false;
        currentPage = menuPage;
        return;
    }

    if (getButtonValue(BTN_B) == PRESSED && bReady) {
        bReady = false;
        String modeOpts[] = { "Eco", "Drive", "Turbo" };
        int choice = openPopupMultiChoice("Select a Mode", modeOpts, 3, currentMode);
        if (choice >= 0) {
            currentMode = (uint8_t)constrain(choice, 0, MODE_COUNT - 1);
            packet.driveMode = currentMode;
            prefs.putUChar("mode", currentMode);
        }
        bool btnDownNow = (digitalRead(PIN_ENC_SW) == LOW);
        pressInProgress = btnDownNow;
        holdTriggered = btnDownNow;
        return;
    }

    if (getButtonValue(BTN_C) == PRESSED && cReady) {
        cReady = false;
        frontLightCmd = frontLightCmd ? 0 : 1;
        rearLightCmd = frontLightCmd;
        saveProfiles();
        bool btnDownNow = (digitalRead(PIN_ENC_SW) == LOW);
        pressInProgress = btnDownNow;
        holdTriggered = btnDownNow;
        return;
    }

    bool btnDown = (digitalRead(PIN_ENC_SW) == LOW);
    if (btnDown && !pressInProgress) {
        pressInProgress = true;
        holdTriggered = false;
    }
    if (pressInProgress && getRotaryEncoderHeld()) {
        holdTriggered = true;
        armed = !armed;
        if (!armed) {
            packet.throttle = 1500;
            packet.steering = 1500;
            resetThrottleRamp();
        }
    }
    if (!btnDown && pressInProgress) {
        pressInProgress = false;
        if (!holdTriggered) {
            currentPage = menuPage;
            return;
        }
    }

    u8g2.setFont(FONT_TEXT_MONOSPACE);
    u8g2.drawStr(0, 8, "VOID");

    u8g2.setFont(u8g2_font_siji_t_6x10);
    u8g2.drawGlyph(110, 10, tele.connected ? 0xe21a : 0xe217);

    drawStringButton(4, 25, "B", "Mode", FONT_TEXT);
    u8g2.setFont(FONT_BOLD_HEADER);
    u8g2.setFontMode(1);
    const char* modeCompact = "Drive";
    if (currentMode == 0) modeCompact = "Eco";
    else if (currentMode == 2) modeCompact = "Turbo";
    u8g2.drawStr(1, 41, modeCompact);
    u8g2.setFontMode(0);

    drawStringButton(4, 59, "D", "Menu", FONT_TEXT);

    int bx = 38, by = 12, bw = 52, bh = 52;
    u8g2.drawRFrame(bx, by, bw, bh, 5);

    int ccx = bx + bw - 9;
    int ccy = by + 8;
    if (mpuAvailable) {
        float ang = ((-mpuHeadingDeg) - 90.0f) * (PI / 180.0f);
        int px = ccx + (int)(cosf(ang) * 4.0f);
        int py = ccy + (int)(sinf(ang) * 4.0f);
        u8g2.drawLine(ccx, ccy, px, py);
        u8g2.drawDisc(px, py, 1);
    } else {
        u8g2.drawLine(ccx - 2, ccy, ccx + 2, ccy);
    }

    if (!armed || !tele.connected) {
        u8g2.setFont(FONT_BOLD_HEADER);
        u8g2.drawStr(bx + 4, by + 13, "!");
    }

    char spdBuf[12];
    if (tele.connected) snprintf(spdBuf, sizeof(spdBuf), "%d", (int)tele.speedKmh);
    else                snprintf(spdBuf, sizeof(spdBuf), "--");
    const char* unit = "km/h";
    u8g2.setFont(FONT_BOLD_HEADER);
    int sw = u8g2.getStrWidth(spdBuf);
    u8g2.drawStr(bx + (bw - sw) / 2, by + 34, spdBuf);
    u8g2.setFont(FONT_TEXT_MONOSPACE);
    int uw = u8g2.getStrWidth(unit);
    u8g2.drawStr(bx + (bw - uw) / 2, by + 46, unit);

    int textX = 109;
    int textY = 24;
    int offset = 13;
    int iconColX = 96;

    u8g2.setFont(FONT_TEXT_MONOSPACE);
    uint8_t battPct = tele.connected ? tele.battPct : 0;

    const int battX = iconColX + 1;
    const int battY = textY - 6;
    const int battW = 10;
    const int battH = 6;
    const int rightColCenterX = battX + (battW / 2);
    u8g2.drawFrame(battX, battY, battW, battH);
    u8g2.drawBox(battX + battW, battY + 2, 1, 2);
    int fillW = map((int)battPct, 0, 100, 0, battW - 2);
    fillW = constrain(fillW, 0, battW - 2);
    if (tele.connected && fillW > 0) {
        u8g2.drawBox(battX + 1, battY + 1, fillW, battH - 2);
    }

    drawStringButton(rightColCenterX, textY + offset * 2 - 1, "C", "", FONT_TEXT);

    totalDrawTime = millis() - startTime;
}
