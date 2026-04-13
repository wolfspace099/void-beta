#pragma once

#include <Arduino.h>
#include "Helpers.h"

enum ButtonId {
    BTN_A = 0,
    BTN_B = 1,
    BTN_C = 2,
    BTN_D = 3,
};

void setupInputs();
void inputsTick();

int  getRotaryEncoderSpins();
int  getRotaryEncoderTotalSpins();
int  getRotaryEncoderSwitchValue();
bool getRotaryEncoderHeld(uint32_t ms = 600);
bool consumeUserActivity();
int  getButtonValue(ButtonId button);

uint16_t getThrottlePWM();
uint16_t getSteerPWM();
void     resetThrottleRamp();
void     readControllerBatt();
