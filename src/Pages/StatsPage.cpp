#include <Arduino.h>
#include "Page.h"
#include "Screen.h"
#include "Inputs.h"
#include "Globals.h"
#include "Helpers.h"

void StatsPage::init() {
    rotaryEncoderButtonReady = false;
    rotaryEncoderSwitchValue = UNPRESSED;
    hovered = 0;
    getRotaryEncoderSpins();
}

void StatsPage::loop() {
    static bool aReady = true;
    if (getButtonValue(BTN_A) == UNPRESSED) aReady = true;
    if (getButtonValue(BTN_A) == PRESSED && aReady) {
        aReady = false;
        currentPage = menuPage;
        return;
    }

    rotaryEncoderSwitchValue = getRotaryEncoderSwitchValue();
    if (rotaryEncoderSwitchValue == UNPRESSED) rotaryEncoderButtonReady = true;
    drawPageHeader("< Home < Menu < ", "Stats");
    u8g2.setFont(FONT_TEXT_MONOSPACE);

    uint32_t sessionSec = (millis() - bootMillis) / 1000UL;
    uint32_t totalSec = statsRunSeconds + sessionSec;

    char statLines[8][28];
    snprintf(statLines[0], sizeof(statLines[0]), "Top Speed: %d km/h", (int)statsTopSpeedKmh);
    snprintf(statLines[1], sizeof(statLines[1]), "Live Speed: %d km/h", (int)tele.speedKmh);
    snprintf(statLines[2], sizeof(statLines[2]), "RPM: %u", (unsigned)tele.rpm);
    snprintf(statLines[3], sizeof(statLines[3]), "RX Batt: %u%%", (unsigned)tele.battPct);
    snprintf(statLines[4], sizeof(statLines[4]), "TX Batt: %u%%", (unsigned)ctrlBattPct);
    snprintf(statLines[5], sizeof(statLines[5]), "Session: %lu:%02lu",
             (unsigned long)(sessionSec / 60UL), (unsigned long)(sessionSec % 60UL));
    snprintf(statLines[6], sizeof(statLines[6]), "Total Hrs: %.2f", totalSec / 3600.0f);
    snprintf(statLines[7], sizeof(statLines[7]), "Packets: %u", (unsigned)packet.seq);

    const int totalStats = 8;
    int increment = 0;
    int spins = getRotaryEncoderSpins();
    if (spins > 0) increment = 1;
    if (spins < 0) increment = -1;
    hovered += increment;
    if (hovered >= totalStats) hovered = totalStats - 1;
    if (hovered < 0) hovered = 0;

    for (int i = 0; i < 5; i++) {
        int statIndex = hovered + i;
        if (statIndex < totalStats) {
            u8g2.drawStr(4, 20 + i * 10, statLines[statIndex]);
        }
    }

    drawScrollBar(totalStats, hovered);
}
