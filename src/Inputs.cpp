#include "Inputs.h"
#include "Globals.h"

// Encoder state
static volatile int  rawCount       = 0;
static volatile uint8_t encPrevState = 0;
static int           protectedCount = 0;
static int           previousCount  = 0;
static bool          switchPressed  = false;
static const int     DEBOUNCE_TIME  = 4;
static unsigned long timeSinceReleased = 0;
static const int     ENCODER_DIRECTION = 1; // set to -1 if direction is inverted

static uint32_t holdStart = 0;
static bool     holdFired = false;
static bool     userActivity = false;
static int      lastEncRawCount = 0;
static int      lastThrottleRaw = 0;
static int      lastSteerRaw = 0;
static uint8_t  lastEncSwRaw = UNPRESSED;
static uint8_t  lastBtnRaw[4] = {UNPRESSED, UNPRESSED, UNPRESSED, UNPRESSED};
static bool     activityInit = false;

static int buttonPin(ButtonId button) {
    switch (button) {
        case BTN_A: return PIN_BTN_A;
        case BTN_B: return PIN_BTN_B;
        case BTN_C: return PIN_BTN_C;
        case BTN_D: return PIN_BTN_D;
        default:    return -1;
    }
}

static const ButtonId kButtons[] = { BTN_A, BTN_B, BTN_C, BTN_D };

static uint16_t mapAxisToPwm(int raw,
                             uint16_t mn,
                             uint16_t ctr,
                             uint16_t mx,
                             uint16_t outMin,
                             uint16_t outCtr,
                             uint16_t outMax) {
    // Guard invalid calibration.
    if (!(mn < ctr && ctr < mx)) {
        mn = 0; ctr = 2048; mx = 4095;
    }

    raw = constrain(raw, (int)mn, (int)mx);
    if (abs(raw - (int)ctr) <= 1) return outCtr;

    if (raw >= (int)ctr) {
        return (uint16_t)map(raw, (int)ctr, (int)mx, outCtr, outMax);
    }
    return (uint16_t)map(raw, (int)mn, (int)ctr, outMin, outCtr);
}

void IRAM_ATTR encISR() {
    // Quadrature transition table for robust direction decoding.
    static const int8_t transitionLut[16] = {
        0, -1,  1,  0,
        1,  0,  0, -1,
       -1,  0,  0,  1,
        0,  1, -1,  0
    };

    uint8_t a = digitalRead(PIN_ENC_CLK);
    uint8_t b = digitalRead(PIN_ENC_DT);
    uint8_t encState = (a << 1) | b;
    uint8_t idx = (encPrevState << 2) | encState;
    encPrevState = encState;

    rawCount += transitionLut[idx];
}

void setupInputs() {
    pinMode(PIN_ENC_CLK, INPUT_PULLUP);
    pinMode(PIN_ENC_DT,  INPUT_PULLUP);
    pinMode(PIN_ENC_SW,  INPUT_PULLUP);
    pinMode(PIN_THROTTLE, INPUT);
    pinMode(PIN_STEER, INPUT);
    for (uint8_t i = 0; i < 4; i++) {
        int pin = buttonPin(kButtons[i]);
        if (pin >= 0) pinMode(pin, INPUT_PULLUP);
    }

    analogReadResolution(12);
#if defined(ESP32)
    analogSetPinAttenuation(PIN_THROTTLE, ADC_11db);
    analogSetPinAttenuation(PIN_STEER, ADC_11db);
#endif

    uint8_t a = digitalRead(PIN_ENC_CLK);
    uint8_t b = digitalRead(PIN_ENC_DT);
    encPrevState = (a << 1) | b;

    attachInterrupt(digitalPinToInterrupt(PIN_ENC_CLK), encISR, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_ENC_DT),  encISR, CHANGE);
}

void inputsTick() {
    // hold tracking
    bool btnRaw = (digitalRead(PIN_ENC_SW) == LOW);
    if (btnRaw  && holdStart == 0) { holdStart = millis(); holdFired = false; }
    if (!btnRaw)                     holdStart = 0;

    every(2000) { readControllerBatt(); }

    // Track user activity for display sleep/wake without consuming UI events.
    int encNow;
    noInterrupts();
    encNow = rawCount;
    interrupts();

    uint8_t encSwNow = digitalRead(PIN_ENC_SW);
    uint8_t btnNow[4];
    for (uint8_t i = 0; i < 4; i++) {
        btnNow[i] = getButtonValue(kButtons[i]);
    }
    int throttleNow = analogRead(PIN_THROTTLE);
    int steerNow = analogRead(PIN_STEER);

    if (!activityInit) {
        lastEncRawCount = encNow;
        lastEncSwRaw = encSwNow;
        for (uint8_t i = 0; i < 4; i++) {
            lastBtnRaw[i] = btnNow[i];
        }
        lastThrottleRaw = throttleNow;
        lastSteerRaw = steerNow;
        activityInit = true;
        return;
    }

    const int axisWakeThreshold = 40;
    bool movedAxis = (abs(throttleNow - lastThrottleRaw) > axisWakeThreshold) ||
                     (abs(steerNow - lastSteerRaw) > axisWakeThreshold);
    bool rotatedEncoder = (encNow != lastEncRawCount);
    bool pressedEncoder = (encSwNow != lastEncSwRaw);
    bool pressedButtons = false;
    for (uint8_t i = 0; i < 4; i++) {
        if (btnNow[i] != lastBtnRaw[i]) {
            pressedButtons = true;
            break;
        }
    }

    if (movedAxis || rotatedEncoder || pressedEncoder || pressedButtons) {
        userActivity = true;
    }

    lastEncRawCount = encNow;
    lastEncSwRaw = encSwNow;
    for (uint8_t i = 0; i < 4; i++) {
        lastBtnRaw[i] = btnNow[i];
    }
    lastThrottleRaw = throttleNow;
    lastSteerRaw = steerNow;
}

int getRotaryEncoderSpins() {
    noInterrupts();
    protectedCount = rawCount / 4;
    interrupts();
    int spins    = protectedCount - previousCount;
    previousCount = protectedCount;
    return spins * ENCODER_DIRECTION;
}

int getRotaryEncoderTotalSpins() {
    return (rawCount / 4) * ENCODER_DIRECTION;
}

int getRotaryEncoderSwitchValue() {
    uint8_t val = digitalRead(PIN_ENC_SW);

    if (val == PRESSED && !switchPressed) {
        if (millis() - timeSinceReleased > (unsigned long)DEBOUNCE_TIME) {
            switchPressed = true;
            return PRESSED;
        }
    }
    if (val == UNPRESSED && switchPressed) {
        switchPressed      = false;
        timeSinceReleased  = millis();
        return UNPRESSED;
    }
    return UNPRESSED;
}

bool getRotaryEncoderHeld(uint32_t ms) {
    bool btnRaw = (digitalRead(PIN_ENC_SW) == LOW);
    if (btnRaw && !holdFired && holdStart > 0 && (millis() - holdStart >= ms)) {
        holdFired = true;
        return true;
    }
    return false;
}

bool consumeUserActivity() {
    bool active = userActivity;
    userActivity = false;
    return active;
}

int getButtonValue(ButtonId button) {
    int pin = buttonPin(button);
    if (pin < 0) return UNPRESSED;
    return (digitalRead(pin) == LOW) ? PRESSED : UNPRESSED;
}

// Snap a PWM output value to clean endpoints/center if within `band` microseconds.
// e.g. 1988 -> 2000, 1503 -> 1500, 1012 -> 1000
static uint16_t pwmDeadband(uint16_t v, uint16_t lo, uint16_t ctr, uint16_t hi, uint16_t band) {
    if (v <= lo + band)              return lo;
    if (v >= hi - band)              return hi;
    if (abs((int)v - (int)ctr) <= (int)band) return ctr;
    return v;
}

// Throttle ramp-down rate: max µs drop per call (~20ms packet interval).
// 15 µs/call = 750 µs/sec — goes from 2000→1500 in ~0.67s. Tune as needed.
#define THROTTLE_RAMP_DOWN_RATE  15

static uint16_t sThrottleOutput = 0;  // last sent throttle value

// RC inputs
uint16_t getThrottlePWM() {
    DriveMode& m = driveModes[currentMode];
    int raw = analogRead(PIN_THROTTLE);
    if (abs(raw - (int)throttleCalCenter) < (int)axisDeadzone) raw = throttleCalCenter;
    uint16_t v = mapAxisToPwm(raw,
                              throttleCalMin,
                              throttleCalCenter,
                              throttleCalMax,
                              m.minPWM,
                              m.midPWM,
                              m.maxPWM);
    if (invertThrottle) v = (uint16_t)((int)m.minPWM + (int)m.maxPWM) - v;
    v = constrain(v, m.minPWM, m.maxPWM);
    v = pwmDeadband(v, m.minPWM, m.midPWM, m.maxPWM, 10);

    // Init on first call
    if (sThrottleOutput == 0) sThrottleOutput = v;

    if (v >= sThrottleOutput) {
        // Rising or holding: always instant
        sThrottleOutput = v;
    } else if (v <= m.midPWM) {
        // Falling to/below neutral: instant (safety cut)
        sThrottleOutput = v;
    } else {
        // Falling but still above neutral: rate-limit the drop (coast-down feel)
        uint16_t maxDrop = (uint16_t)THROTTLE_RAMP_DOWN_RATE;
        if (sThrottleOutput - v > maxDrop) {
            sThrottleOutput -= maxDrop;
        } else {
            sThrottleOutput = v;
        }
    }

    return sThrottleOutput;
}

uint16_t getSteerPWM() {
    int raw = analogRead(PIN_STEER);
    if (abs(raw - (int)steerCalCenter) < (int)axisDeadzone) raw = steerCalCenter;
    uint16_t v = mapAxisToPwm(raw,
                              steerCalMin,
                              steerCalCenter,
                              steerCalMax,
                              1200,
                              steerCtrPwm,
                              2200);
    if (invertSteer) v = (uint16_t)(1200 + 2200) - v;  // mirror around axis midpoint
    v = constrain(v, 1000, 2200);
    return pwmDeadband(v, 1200, steerCtrPwm, 2200, 10);
}

void resetThrottleRamp() {
    sThrottleOutput = 0;  // will re-init to current value on next getThrottlePWM() call
}

void readControllerBatt() {
    // TX board has no local battery divider in this setup.
    ctrlBattVolt = 0.0f;
    ctrlBattPct  = 0;
}
