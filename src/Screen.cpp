#include "Screen.h"
#include "Globals.h"
#include "Helpers.h"

static uint8_t bootAnimTick = 0;

void bootStatus(const char* status, uint8_t step, uint8_t total, uint16_t holdMs) {
    u8g2.clearBuffer();

    u8g2.setFont(FONT_BOLD_HEADER);
    const char* title = "VOID";
    int titleW = u8g2.getStrWidth(title);
    int titleX = (128 - titleW) / 2;
    u8g2.drawStr(titleX, 18, title);

    u8g2.setFont(FONT_TEXT);
    int statusW = u8g2.getStrWidth(status);
    int statusX = (128 - statusW) / 2;
    u8g2.drawStr(statusX, 40, status);

    // 10x4 progress square/segment.
    const int ix = (128 - 10) / 2;
    const int iy = 56;
    u8g2.drawFrame(ix, iy, 10, 4);
    int fillW = 1;
    if (total > 0 && step <= total) {
        fillW = map((int)step, 0, (int)total, 1, 8);
    } else {
        fillW = 1 + (bootAnimTick % 8);
    }
    u8g2.drawBox(ix + 1, iy + 1, fillW, 2);

    bootAnimTick++;
    u8g2.sendBuffer();
    delay(holdMs);
}

void setupScreen() {
    u8g2.begin();
    u8g2.setContrast(255);
    bootStatus("Setting up screen", 1, 7, 420);
}

void finishBootScreen() {
    delay(180);
    u8g2.clearBuffer();
    u8g2.sendBuffer();
}
