#include <Arduino.h>
#include "Page.h"
#include "Screen.h"
#include "Inputs.h"
#include "Globals.h"
#include "Helpers.h"

void ModesPage::init() {
    rotaryEncoderButtonReady = false;
    rotaryEncoderSwitchValue = UNPRESSED;
    hovered = currentMode;
    getRotaryEncoderSpins();
}

void ModesPage::loop() {
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
    if (spins > 0 && hovered < MODE_COUNT - 1) hovered++;
    if (spins < 0 && hovered > 0)              hovered--;

    drawPageHeader("< Home < Menu < ", "Modes");

    // Left list (reference-like compact list)
    int rowSpacing      = 14;
    int listYStart      = 24;
    int listLeftSpacing = 8;

    u8g2.setFont(FONT_TEXT);
    for (int i = 0; i < MODE_COUNT; i++) {
        const char* modeLabel = "Drive";
        if (i == 0) modeLabel = "Eco";
        else if (i == 2) modeLabel = "Turbo";

        if (i == (int)currentMode) u8g2.drawStr(listLeftSpacing, listYStart, ">");
        u8g2.drawStr(listLeftSpacing + 10, listYStart, modeLabel);
        if (hovered == i) {
            int tw = u8g2.getStrWidth(modeLabel);
            u8g2.drawRFrame(listLeftSpacing + 8, listYStart - 10, tw + 6, 13, 5);
        }

        listYStart += rowSpacing;
    }

    // Right visual panel: estimated top speed for hovered mode.
    const int panelX = 72, panelY = 14, panelW = 54, panelH = 49;
    DriveMode& dm = driveModes[hovered];
    int modeSpan = (int)dm.maxPWM - (int)dm.minPWM;
    int previewVal = map(modeSpan, 400, 1000, 18, 46);
    previewVal = constrain(previewVal, 12, 60);
    const char* previewUnit = "km/h";

    char speedBuf[10];
    snprintf(speedBuf, sizeof(speedBuf), "%d", previewVal);
    u8g2.setFont(FONT_BOLD_HEADER);
    int sw = u8g2.getStrWidth(speedBuf);
    u8g2.drawStr(panelX + (panelW - sw) / 2, panelY + 24, speedBuf);
    u8g2.setFont(FONT_TINY_TEXT);
    int uw = u8g2.getStrWidth(previewUnit);
    u8g2.drawStr(panelX + (panelW - uw) / 2, panelY + 31, previewUnit);

    const int barX = panelX + 7;
    const int barY = panelY + 37;
    const int barW = panelW - 14;
    const int barH = 8;
    u8g2.drawFrame(barX, barY, barW, barH);
    int fillW = map(previewVal, 12, 60, 1, barW - 2);
    fillW = constrain(fillW, 1, barW - 2);
    u8g2.drawBox(barX + 1, barY + 1, fillW, barH - 2);

    if (rotaryEncoderSwitchValue == PRESSED && rotaryEncoderButtonReady) {
        rotaryEncoderButtonReady = false;
        currentMode      = hovered;
        packet.driveMode = currentMode;
        prefs.putUChar("mode", currentMode);
        currentPage = menuPage;
    }

}
