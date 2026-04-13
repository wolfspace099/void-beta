#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "Globals.h"

void setupScreen();
void bootStatus(const char* status, uint8_t step = 0, uint8_t total = 0, uint16_t holdMs = 320);
void finishBootScreen();
