#pragma once

#include <Arduino.h>
#include "Globals.h"

#define every(interval) \
    static uint32_t __every__##interval = millis(); \
    if (millis() - __every__##interval >= interval && (__every__##interval = millis()))

float mapFloat(float x, float in_min, float in_max, float out_min, float out_max);

void drawWrappedStr(const char* text, int x, int y, int maxWidth, bool centerAlign, int lineSpacing);

void drawPageHeader(String breadcrumb, String pageName);
void drawStringButton(int x, int y, String icon, String label, const uint8_t* font);
void drawScrollBar(int totalItems, int hoveredIndex);
void drawGrid();
