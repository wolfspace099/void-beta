#include <Arduino.h>
#include "Page.h"
#include "Screen.h"
#include "Inputs.h"
#include "Globals.h"
#include "Helpers.h"

static const uint8_t ICON_CONTROLS_12X12[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x03, 0xbe, 0x07, 0xfb, 0x0f,
    0xf1, 0x09, 0xfb, 0x09, 0x0f, 0x0f, 0x06, 0x06, 0x00, 0x00, 0x00, 0x00
};

static const uint8_t ICON_STATS_12X12[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x00, 0x60, 0x00, 0x60, 0x00,
    0x60, 0x03, 0x60, 0x03, 0x6c, 0x03, 0x6c, 0x03, 0x6c, 0x03, 0x00, 0x00
};

struct MenuItem {
    const char* label;
    const uint8_t* icon;
    uint8_t iconType; // 0=xbmp, 1=twelvedings glyph, 2=siji glyph
    uint16_t iconCode;
    uint8_t id;
};

struct MenuSlot {
    uint8_t x;
    uint8_t row;
};

static const MenuItem items[] = {
    { "Controls",  ICON_CONTROLS_12X12, 0, 0x0000, 3 },
    { "Modes",     nullptr,             2, 0xe09e, 0 },
    { "Settings",  nullptr,             1, 0x0047, 1 },
    { "Stats",     ICON_STATS_12X12,    0, 0x0000, 2 },
    { "Calibrate", nullptr,             1, 0x007e, 4 }, // tuning-wrench glyph
};

static const MenuSlot slots[] = {
    { 4,  0 },   // Controls
    { 66, 0 },   // Modes
    { 4,  1 },   // Settings
    { 66, 1 },   // Stats
    { 4,  2 },   // Calibrate
};

void MenuPage::init() {
    backButtonReady          = (getButtonValue(BTN_A) == UNPRESSED);
    rotaryEncoderButtonReady = false;
    rotaryEncoderSwitchValue = UNPRESSED;
    hovered = 0;
    getRotaryEncoderSpins();
}

static void drawTile(int x, int y, bool selected, const MenuItem& it) {
    if (it.iconType == 1) {
        u8g2.setFont(u8g2_font_twelvedings_t_all);
        u8g2.drawGlyph(x + 3, y + 12, it.iconCode);
    } else if (it.iconType == 2) {
        u8g2.setFont(u8g2_font_siji_t_6x10);
        u8g2.drawGlyph(x + 3, y + 12, it.iconCode);
    } else if (it.icon) {
        u8g2.drawXBMP(x + 2, y + 2, 12, 12, it.icon);
    }

    u8g2.setFont(FONT_TEXT_MONOSPACE);
    u8g2.drawStr(x + 18, y + 11, it.label);
    if (selected) {
        int textW = u8g2.getStrWidth(it.label);
        int frameW = textW + 24;
        u8g2.drawRFrame(x - 2, y - 2, frameW, 16, 5);
    }
}

void MenuPage::loop() {
    if (getButtonValue(BTN_A) == UNPRESSED) backButtonReady = true;
    if (getButtonValue(BTN_A) == PRESSED && backButtonReady) {
        backButtonReady = false;
        currentPage = hudPage;
        return;
    }

    rotaryEncoderSwitchValue = getRotaryEncoderSwitchValue();
    if (rotaryEncoderSwitchValue == UNPRESSED && !rotaryEncoderButtonReady)
        rotaryEncoderButtonReady = true;

    int spins = getRotaryEncoderSpins();
    const int itemCount = (int)(sizeof(items) / sizeof(items[0]));
    if (spins > 0) hovered = (hovered + 1) % itemCount;
    if (spins < 0) hovered = (hovered - 1 + itemCount) % itemCount;

    drawPageHeader("< Home < ", "Menu");

    const int rowSpacing = 15;  // tighter to fit 3 rows
    const int baseY = 22;
    for (int i = 0; i < itemCount; i++) {
        int y = baseY - 9 + rowSpacing * slots[i].row;
        drawTile(slots[i].x, y, hovered == i, items[i]);
    }

    if (rotaryEncoderSwitchValue == PRESSED && rotaryEncoderButtonReady) {
        rotaryEncoderButtonReady = false;
        switch (items[hovered].id) {
            case 0: currentPage = modesPage;     break;
            case 1: currentPage = settingsPage;  break;
            case 2: currentPage = statsPage;     break;
            case 3: currentPage = controlsPage;  break;
            case 4: currentPage = calibratePage; calibratePage->init(); break;
            default: currentPage = hudPage;      break;
        }
        return;
    }
}
