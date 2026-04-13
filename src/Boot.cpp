#include "Boot.h"
#include "Globals.h"

// VOID logo glyphs - Agency FB style, 5 cols x 10 rows, bit0=top
static const uint8_t glyph_V[5] = { 0b1111111, 0b0011110, 0b0001100, 0b0011110, 0b1111111 };
static const uint8_t glyph_O[5] = { 0b1111111, 0b1000001, 0b1000001, 0b1000001, 0b1111111 };
static const uint8_t glyph_I[5] = { 0b1111111, 0b0011100, 0b0011100, 0b0011100, 0b1111111 };
static const uint8_t glyph_D[5] = { 0b1111111, 0b1000001, 0b1000001, 0b0111110, 0b0000000 };

static void drawGlyph(const uint8_t* g, int ox, int oy, int sc) {
    for (int col = 0; col < 5; col++)
        for (int row = 0; row < 10; row++)
            if (g[col] & (1 << row))
                u8g2.drawBox(ox + col * sc, oy + row * sc, sc, sc);
}

static void drawVOID(int ox, int oy, int sc) {
    int step = (5 + 2) * sc;
    drawGlyph(glyph_V, ox + 0 * step, oy, sc);
    drawGlyph(glyph_O, ox + 1 * step, oy, sc);
    drawGlyph(glyph_I, ox + 2 * step, oy, sc);
    drawGlyph(glyph_D, ox + 3 * step, oy, sc);
}

void showBootScreen() {
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    delay(150);

    // Phase 1: loading dots, 3 pulses
    for (int d = 1; d <= 3; d++) {
        u8g2.clearBuffer();
        u8g2.setFont(FONT_TEXT);
        u8g2.drawStr(34, 36, "loading");
        for (int i = 0; i < d; i++)
            u8g2.drawDisc(34 + 56 + i * 8, 32, 2);
        u8g2.sendBuffer();
        delay(350);
        u8g2.clearBuffer();
        u8g2.sendBuffer();
        delay(60);
    }
    delay(150);

    // Phase 2: VOID slides in from top
    int logoX    = 8;
    int logoFinal = 12;
    int logoH    = 10 * 4; // GLYPH_H * SCALE

    for (int y = -logoH; y <= logoFinal; y += 4) {
        u8g2.clearBuffer();
        drawVOID(logoX, y, 4);
        u8g2.sendBuffer();
        delay(16);
    }

    // Hold + author tag
    u8g2.setFont(FONT_TEXT);
    u8g2.drawStr(38, 60, "by Cat");
    u8g2.sendBuffer();
    delay(1000);

    // Blink out
    for (int i = 0; i < 3; i++) {
        u8g2.clearBuffer(); u8g2.sendBuffer(); delay(70);
        drawVOID(logoX, logoFinal, 4);
        u8g2.setFont(FONT_TEXT);
        u8g2.drawStr(38, 60, "by Cat");
        u8g2.sendBuffer(); delay(70);
    }
    u8g2.clearBuffer(); u8g2.sendBuffer(); delay(80);
}

