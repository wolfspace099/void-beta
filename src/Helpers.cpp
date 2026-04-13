#include <Arduino.h>
#include "Helpers.h"
#include "Globals.h"
#include "Screen.h"

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void drawPageHeader(String breadcrumb, String pageName) {
    u8g2.setFont(FONT_TINY_TEXT);
    u8g2.drawStr(4, 8, "A");
    u8g2.drawRFrame(1, 1, 9, 9, 3);

    u8g2.drawStr(12, 9, breadcrumb.c_str());
    int width = u8g2.getStrWidth(breadcrumb.c_str());

    u8g2.setFont(FONT_HEADER);
    u8g2.drawStr(width + 12, 9, pageName.c_str());

    u8g2.drawHLine(-1, 11, 130);
}

void drawStringButton(int x, int y, String icon, String label, const uint8_t* font) {
    u8g2.setFont(FONT_TINY_TEXT);
    u8g2.drawStr(x - 1, y + 3, icon.c_str());
    u8g2.drawRFrame(x - 4, y - 4, 9, 9, 3);

    u8g2.setFont(font);
    u8g2.drawStr(x + 7, y + 4, label.c_str());
}

void drawScrollBar(int totalItems, int hoveredIndex) {
    int minHeight      = 4;
    int maxHeight      = 40;
    int scrollBarHeight = map(totalItems, 4, 20, maxHeight, minHeight);
    scrollBarHeight     = constrain(scrollBarHeight, minHeight, maxHeight);
    int scrollBarY      = map(hoveredIndex, 0, totalItems - 1, 13, 64 - scrollBarHeight);
    u8g2.drawBox(0, scrollBarY, 2, scrollBarHeight);
}

void drawGrid() {
    for (int x = 0; x < 128; x += 10)
        for (int y = 0; y < 64; y += 10) {
            if (x % 50 == 0 && y % 50 == 0)
                u8g2.drawBox(x - 1, y - 1, 3, 3);
            else
                u8g2.drawPixel(x, y);
        }
}

void drawWrappedStr(const char* text, int x, int y, int maxWidth, bool centerAlign, int lineSpacing) {
    if (text == nullptr || maxWidth <= 0) return;

    int lineHeight = u8g2.getMaxCharHeight() + lineSpacing;
    int cursorY    = y;
    const char* wordStart = text;

    while (*wordStart) {
        int lineWidth     = 0;
        const char* lineStart = wordStart;
        const char* wordEnd;
        char wordBuffer[50];

        while (*wordStart) {
            wordEnd = wordStart;
            while (*wordEnd && *wordEnd != ' ') wordEnd++;
            int wordLength = wordEnd - wordStart;
            if (wordLength > 49) wordLength = 49;
            strncpy(wordBuffer, wordStart, wordLength);
            wordBuffer[wordLength] = '\0';
            int wordWidth = u8g2.getStrWidth(wordBuffer);
            if (lineWidth + wordWidth > maxWidth) {
                if (lineWidth == 0) {
                    lineWidth = wordWidth;
                    if (*wordEnd == ' ') wordEnd++;
                    wordStart = wordEnd;
                }
                break;
            }
            lineWidth += wordWidth;
            if (*wordEnd == ' ') { lineWidth += u8g2.getStrWidth(" "); wordEnd++; }
            wordStart = wordEnd;
        }

        int cursorX = x;
        if (centerAlign) cursorX = x + (maxWidth - lineWidth) / 2;

        while (lineStart < wordStart) {
            wordEnd = lineStart;
            while (*wordEnd && *wordEnd != ' ') wordEnd++;
            int wordLength = wordEnd - lineStart;
            if (wordLength > 49) wordLength = 49;
            strncpy(wordBuffer, lineStart, wordLength);
            wordBuffer[wordLength] = '\0';
            u8g2.setCursor(cursorX, cursorY);
            u8g2.print(wordBuffer);
            cursorX += u8g2.getStrWidth(wordBuffer);
            if (*wordEnd == ' ') { cursorX += u8g2.getStrWidth(" "); wordEnd++; }
            lineStart = wordEnd;
        }
        cursorY += lineHeight;
    }
}
