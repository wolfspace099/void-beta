#include <Arduino.h>
#include "Popup.h"
#include "Screen.h"
#include "Inputs.h"
#include "Globals.h"
#include "Helpers.h"

static void waitForEncoderRelease() {
    uint32_t start = millis();
    while (digitalRead(PIN_ENC_SW) == LOW) {
        if (millis() - start > 600) break;
        delay(1);
    }
}

int openPopupMultiChoice(String header, String choices[], int numChoices, int hovered) {
    getRotaryEncoderSpins(); // flush

    int selection = -1;
    header = " " + header + " ";
    u8g2.setFont(FONT_HEADER);
    int headerWidth = u8g2.getStrWidth(header.c_str());

    bool encReady     = false;
    int  encSwitchVal = UNPRESSED;

    while (selection == -1) {
        encSwitchVal = getRotaryEncoderSwitchValue();
        if (encSwitchVal == UNPRESSED) encReady = true;
        if (getButtonValue(BTN_A) == PRESSED) return -1;

        u8g2.clearBuffer();

        u8g2.drawRFrame(1, 1, 126, 62, 4);

        u8g2.setFont(FONT_HEADER);
        u8g2.drawStr(64 - (headerWidth / 2), 14, header.c_str());

        int currentY = 31;
        int rowStep = 14;
        int hGap = 14;
        int totalTextW = 0;
        for (int i = 0; i < numChoices; i++) totalTextW += u8g2.getStrWidth(choices[i].c_str());
        int compactGap = 10;
        int normalGap = 14;
        int chosenGap = normalGap;
        if (numChoices > 1) {
            int oneRowCompact = totalTextW + compactGap * (numChoices - 1);
            if (oneRowCompact <= 100) chosenGap = compactGap;
        }
        hGap = chosenGap;
        int rowStartX = 18;
        int oneRowW = totalTextW + hGap * (numChoices - 1);
        if (oneRowW <= 110) rowStartX = (128 - oneRowW) / 2;
        int currentX  = rowStartX;
        u8g2.setFont(FONT_TEXT);
        const int textAscent = u8g2.getAscent();
        const int textDescent = u8g2.getDescent();
        const int textHeight = textAscent - textDescent;
        for (int i = 0; i < numChoices; i++) {
            int textW = u8g2.getStrWidth(choices[i].c_str());
            int choiceWidth = textW + hGap;
            if (currentX + choiceWidth > 128) {
                currentX  = rowStartX;
                currentY += rowStep;
            }
            u8g2.drawStr(currentX, currentY, choices[i].c_str());
            if (hovered == i) {
                int pillX = currentX - 4;
                int pillY = currentY - textAscent - 2;
                int pillW = textW + 8;
                int pillH = textHeight + 4;

                if (pillX < 2) pillX = 2;
                if (pillY < 18) pillY = 18;
                if (pillX + pillW > 126) pillW = 126 - pillX;
                if (pillY + pillH > 61) pillH = 61 - pillY;
                if (pillW > 4 && pillH > 4) u8g2.drawRFrame(pillX, pillY, pillW, pillH, 5);
            }
            currentX += choiceWidth;
        }

        u8g2.sendBuffer();

        int increment = 0;
        int spins     = getRotaryEncoderSpins();
        if (spins > 0) increment =  1;
        if (spins < 0) increment = -1;

        hovered += increment;
        if (hovered >= numChoices) hovered = numChoices - 1;
        else if (hovered < 0)     hovered = 0;

        if (encSwitchVal == PRESSED && encReady) {
            waitForEncoderRelease();
            selection = hovered;
        }
    }
    return selection;
}

long openPopupNumber(String header, long initialValue, long minValue, long maxValue) {
    getRotaryEncoderSpins();

    long value = initialValue;
    header     = " " + header + " ";
    u8g2.setFont(FONT_HEADER);
    int headerWidth = u8g2.getStrWidth(header.c_str());

    bool encReady     = false;
    int  encSwitchVal = UNPRESSED;

    unsigned long lastIncrTime    = 0;
    unsigned long maxScrollDelay  = 200;
    unsigned long curScrollDelay  = maxScrollDelay;
    unsigned long minScrollDelay  = 30;

    while (true) {
        encSwitchVal = getRotaryEncoderSwitchValue();
        if (encSwitchVal == UNPRESSED) encReady = true;
        if (getButtonValue(BTN_A) == PRESSED) return initialValue;

        u8g2.clearBuffer();

        u8g2.drawRFrame(1, 1, 126, 62, 4);

        int xOff = 20, yOff = 14;
        u8g2.setFont(FONT_HEADER);
        u8g2.drawStr(64 - (headerWidth / 2) + xOff, 14 + yOff, header.c_str());

        u8g2.setFont(FONT_TEXT_MONOSPACE);
        String valStr  = String(value);
        int    valW    = u8g2.getStrWidth(valStr.c_str());
        int    valX    = 64 - (valW / 2) + xOff;
        u8g2.drawLine(valX, 32 + yOff, valX + valW - 2, 32 + yOff);
        u8g2.drawStr(valX, 30 + yOff, valStr.c_str());

        int buttonSpacing = 14;
        drawStringButton(10, 10, "A", "Back", FONT_TEXT);
        drawStringButton(10, 10 + buttonSpacing, "B", "-100", FONT_TEXT_MONOSPACE);
        drawStringButton(10, 10 + buttonSpacing * 2, "C", "+100", FONT_TEXT_MONOSPACE);
        drawStringButton(10, 10 + buttonSpacing * 3, "D", "Save", FONT_TEXT);

        u8g2.sendBuffer();

        (void)getRotaryEncoderSpins();
        long newValue = value;

        unsigned long now = millis();
        bool anyAdjustBtn = (getButtonValue(BTN_B) == PRESSED) ||
                      (getButtonValue(BTN_C) == PRESSED) ||
                      false;
        if (anyAdjustBtn && (now - lastIncrTime >= curScrollDelay || curScrollDelay == maxScrollDelay)) {
            curScrollDelay = max(minScrollDelay, curScrollDelay - minScrollDelay);
            lastIncrTime = now;

            if (getButtonValue(BTN_B) == PRESSED) {
                newValue = value - 100;
                if (newValue >= minValue) value = newValue;
            } else if (getButtonValue(BTN_C) == PRESSED) {
                newValue = value + 100;
                if (newValue <= maxValue) value = newValue;
            }
        } else if (!anyAdjustBtn) {
            curScrollDelay = maxScrollDelay;
        }

        if (getButtonValue(BTN_D) == PRESSED) return value;
        if (encSwitchVal == PRESSED && encReady) {
            waitForEncoderRelease();
            return value;
        }
    }
}

String openPopupString(String header, String initialValue, int stringLength) {
    getRotaryEncoderSpins();

    String value = initialValue;
    if ((int)value.length() > stringLength)
        value = value.substring(0, stringLength);
    while ((int)value.length() < stringLength)
        value += ' ';

    int  hovered      = 0;
    int  scrollOffset = 0;
    header = " " + header + " ";
    u8g2.setFont(FONT_HEADER);
    int headerWidth = u8g2.getStrWidth(header.c_str());

    bool encReady     = false;
    int  encSwitchVal = UNPRESSED;

    bool          cursorVisible        = true;
    unsigned long lastCursorBlinkTime  = 0;
    const unsigned long cursorBlinkInterval = 500;
    unsigned long lastIncrTime = 0;
    unsigned long maxScrollDelay = 200;
    unsigned long curScrollDelay = maxScrollDelay;
    unsigned long minScrollDelay = 30;

    while (true) {
        encSwitchVal = getRotaryEncoderSwitchValue();
        if (encSwitchVal == UNPRESSED) encReady = true;

        u8g2.clearBuffer();

        u8g2.drawRFrame(1, 1, 126, 62, 4);

        int xOff = 25, yOff = 10;
        u8g2.setFont(FONT_HEADER);
        u8g2.drawStr(64 - (headerWidth / 2) + xOff, 14 + yOff, header.c_str());

        u8g2.setFont(FONT_TEXT);
        int charSpacing = 10;
        int startX      = 64 - ((min(stringLength, 6) * charSpacing) / 2) + xOff;

        for (int i = scrollOffset; i < min(stringLength, scrollOffset + 6); i++) {
            int charX = startX + ((i - scrollOffset) * charSpacing);
            u8g2.drawStr(charX, 32 + yOff, String(value[i]).c_str());

            if (hovered == i && cursorVisible)
                u8g2.drawRFrame(charX - 2, 20 + yOff, charSpacing, 14, 2);

            u8g2.drawLine(charX, 36 + yOff, charX + charSpacing - 4, 36 + yOff);
        }

        int buttonSpacing = 14;
        drawStringButton(10, 10, "A", "Back", FONT_TEXT);
        drawStringButton(10, 10 + buttonSpacing, "B", "-", FONT_TEXT);
        drawStringButton(10, 10 + buttonSpacing * 2, "C", "+", FONT_TEXT);
        drawStringButton(10, 10 + buttonSpacing * 3, "D", "Save", FONT_TEXT);

        u8g2.sendBuffer();

        int spins = getRotaryEncoderSpins();
        if (spins > 0) hovered++;
        if (spins < 0) hovered--;
        hovered = constrain(hovered, 0, stringLength - 1);
        if (hovered < scrollOffset) scrollOffset = hovered;
        else if (hovered >= scrollOffset + 6) scrollOffset = hovered - 5;

        unsigned long now = millis();

        bool minusPressed = (getButtonValue(BTN_B) == PRESSED);
        bool plusPressed = (getButtonValue(BTN_C) == PRESSED);
        bool anyAdjustPressed = plusPressed || minusPressed;
        if (anyAdjustPressed && (now - lastIncrTime >= curScrollDelay || curScrollDelay == maxScrollDelay)) {
            int c = value[hovered];
            if (c < 32) c = 32;
            if (c > 126) c = 126;
            if (plusPressed && c < 126) c++;
            if (minusPressed && c > 32) c--;
            value.setCharAt(hovered, (char)c);
            curScrollDelay = max(minScrollDelay, curScrollDelay - minScrollDelay);
            lastIncrTime = now;
            cursorVisible = true;
            lastCursorBlinkTime = now;
        } else if (!anyAdjustPressed) {
            curScrollDelay = maxScrollDelay;
        }

        if (now - lastCursorBlinkTime >= cursorBlinkInterval) {
            cursorVisible       = !cursorVisible;
            lastCursorBlinkTime = now;
        }

        if (getButtonValue(BTN_A) == PRESSED) return initialValue;
        if (getRotaryEncoderHeld(700)) return initialValue;

        if (getButtonValue(BTN_D) == PRESSED) {
            return value.substring(0, stringLength);
        }
        if (encSwitchVal == PRESSED && encReady) {
            waitForEncoderRelease();
            encReady = false;
            return value.substring(0, stringLength);
        }
    }
}
