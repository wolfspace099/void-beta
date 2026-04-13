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

// Draw a small bar indicator for a joystick axis.
// cx/cy = center of bar. value = -100..100.
// horiz = true for horizontal (steer), false for vertical (throttle)
static void drawAxisBar(int cx, int cy, int value, bool horiz) {
    const int halfLen = 10;
    const int thick   = 2;

    if (horiz) {
        u8g2.drawFrame(cx - halfLen, cy - thick, halfLen * 2, thick * 2, 0);
    } else {
        u8g2.drawFrame(cx - thick, cy - halfLen, thick * 2, halfLen * 2, 0);
    }

    int fill = (value * halfLen) / 100;
    if (fill == 0) return;

    if (horiz) {
        int x = (fill > 0) ? cx : cx + fill;
        u8g2.drawBox(x, cy - thick + 1, abs(fill), thick * 2 - 2);
    } else {
        int y = (fill > 0) ? cy - fill : cy;
        u8g2.drawBox(cx - thick + 1, y, thick * 2 - 2, abs(fill));
    }
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

    // ─── Layout ──────────────────────────────────────────────────────────────
    //
    //  [ B Mode ]  [======SPEED======]  [wifi]
    //  [ Eco    ]  [   !!  / heading ]  [batt%]
    //              [    ##  km/h     ]  [C Lgt]
    //              [    km/h         ]
    //  [ D Menu ]
    //  [throt]      [====steer====]
    //
    // Centre box
    const int bx = 38, by = 12, bw = 50, bh = 50;

    // Right column: fixed x so icons + values are aligned
    const int iconX = 97;   // icon glyph left edge
    const int valX  = 111;  // text value left edge
    const int rowH  = 13;

    // ── Right column ─────────────────────────────────────────────────────────
    int ry = 20;

    // Row 0: Link / connection
    u8g2.setFont(u8g2_font_siji_t_6x10);
    u8g2.drawGlyph(iconX, ry, tele.connected ? 0xe21a : 0xe217);
    ry += rowH;

    // Row 1: Battery
    {
        const int battW = 9, battH = 5;
        const int battY = ry - battH;
        u8g2.drawFrame(iconX, battY, battW, battH);
        u8g2.drawBox(iconX + battW, battY + 1, 1, 3);
        uint8_t battPct = tele.connected ? tele.battPct : 0;
        int fillW = constrain((int)map(battPct, 0, 100, 0, battW - 2), 0, battW - 2);
        if (tele.connected && fillW > 0) {
            u8g2.drawBox(iconX + 1, battY + 1, fillW, battH - 2);
        }
        u8g2.setFont(FONT_TEXT_MONOSPACE);
        if (tele.connected) {
            char pctBuf[8];
            snprintf(pctBuf, sizeof(pctBuf), "%d%%", battPct);
            u8g2.drawStr(valX, ry, pctBuf);
        } else {
            u8g2.drawStr(valX, ry, "--");
        }
    }
    ry += rowH;

    // Row 2: Lights toggle (C button)
    {
        // Small button label
        u8g2.setFont(FONT_TINY_TEXT);
        u8g2.drawStr(iconX + 1, ry - 5, "C");
        u8g2.drawRFrame(iconX - 1, ry - 9, 7, 7, 2);
        // Light state indicator: filled circle = on, outline = off
        if (frontLightCmd) {
            u8g2.drawDisc(valX + 3, ry - 4, 3);
        } else {
            u8g2.drawCircle(valX + 3, ry - 4, 3);
        }
        u8g2.setFont(FONT_TINY_TEXT);
        u8g2.drawStr(valX + 8, ry, frontLightCmd ? "ON" : "OFF");
    }

    // ── Centre box ───────────────────────────────────────────────────────────
    u8g2.drawRFrame(bx, by, bw, bh, 5);

    // Compass / heading indicator (top-right corner inside box)
    {
        int ccx = bx + bw - 8;
        int ccy = by + 7;
        if (mpuAvailable) {
            float ang = ((-mpuHeadingDeg) - 90.0f) * (PI / 180.0f);
            int px = ccx + (int)(cosf(ang) * 4.0f);
            int py = ccy + (int)(sinf(ang) * 4.0f);
            u8g2.drawLine(ccx, ccy, px, py);
            u8g2.drawDisc(px, py, 1);
        } else {
            u8g2.drawLine(ccx - 2, ccy, ccx + 2, ccy);
        }
    }

    // Arm / connection warning
    if (!armed || !tele.connected) {
        u8g2.setFont(FONT_BOLD_HEADER);
        u8g2.drawStr(bx + 4, by + 13, "!");
    }

    // Speed
    char spdBuf[12];
    if (tele.connected) snprintf(spdBuf, sizeof(spdBuf), "%d", (int)tele.speedKmh);
    else                snprintf(spdBuf, sizeof(spdBuf), "--");
    const char* unit = "km/h";
    u8g2.setFont(FONT_BOLD_HEADER);
    int sw = u8g2.getStrWidth(spdBuf);
    u8g2.drawStr(bx + (bw - sw) / 2, by + 32, spdBuf);
    u8g2.setFont(FONT_TEXT_MONOSPACE);
    int uw = u8g2.getStrWidth(unit);
    u8g2.drawStr(bx + (bw - uw) / 2, by + 44, unit);

    // ── Left column ───────────────────────────────────────────────────────────
    drawStringButton(4, 25, "B", "Mode", FONT_TEXT);
    u8g2.setFont(FONT_BOLD_HEADER);
    u8g2.setFontMode(1);
    const char* modeCompact = "Drive";
    if (currentMode == 0)      modeCompact = "Eco";
    else if (currentMode == 2) modeCompact = "Turbo";
    u8g2.drawStr(1, 40, modeCompact);
    u8g2.setFontMode(0);

    drawStringButton(4, 59, "D", "Menu", FONT_TEXT);

    // ── Joystick indicators ───────────────────────────────────────────────────
    {
        int throttleRaw = analogRead(PIN_THROTTLE);
        int steerRaw    = analogRead(PIN_STEER);

        // Map raw ADC reading to -100..100 for display, with cosmetic deadzone
        auto toPct = [](int raw, int ctr, int mn, int mx) -> int {
            const int dz = 60;
            if (abs(raw - ctr) < dz) return 0;
            if (raw >= ctr) return (int)map(raw, ctr, mx, 0, 100);
            else            return (int)map(raw, mn, ctr, -100, 0);
        };

        int tPct = toPct(throttleRaw, (int)throttleCalCenter,
                         (int)throttleCalMin, (int)throttleCalMax);
        int sPct = toPct(steerRaw, (int)steerCalCenter,
                         (int)steerCalMin, (int)steerCalMax);

        if (invertThrottle) tPct = -tPct;
        if (invertSteer)    sPct = -sPct;

        tPct = constrain(tPct, -100, 100);
        sPct = constrain(sPct, -100, 100);

        // Throttle: vertical bar left of speed box
        drawAxisBar(30, 37, tPct, false);

        // Steer: horizontal bar below speed box, centered on it
        drawAxisBar(bx + bw / 2, by + bh + 6, sPct, true);
    }

    totalDrawTime = millis() - startTime;
}