#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include "Helpers.h"
#include "Globals.h"
#include "Inputs.h"
#include "Screen.h"
#include "Page.h"
#include "Now.h"

#define UpdateScreenInterval 50
#define RadioSendIntervalMs 20

static Adafruit_MPU6050 mpu;
static uint8_t mpuAddress = 0;
static uint32_t mpuLastSampleUs = 0;

static void refreshPacketState(bool includeLiveSticks) {
    if (includeLiveSticks && armed) {
        packet.throttle = getThrottlePWM();
        packet.steering = getSteerPWM();
    } else {
        packet.throttle = 1500;
        packet.steering = steerCtrPwm;
    }

    packet.version       = RC_PROTOCOL_VERSION;
    packet.driveMode     = currentMode;
    packet.frontLightCmd = frontLightCmd;
    packet.rearLightCmd  = rearLightCmd;
    packet.fanPctCmd     = fanPctCmd;
    packet.armed         = armed;
}

static void setupMPU6050() {
    mpuAvailable = false;
    mpuAddress = 0;

    // Only probe valid MPU6050 addresses to avoid touching OLED or other I2C devices.
    const uint8_t candidates[2] = {0x68, 0x69};
    for (uint8_t i = 0; i < 2; i++) {
        if (mpu.begin(candidates[i], &Wire)) {
            mpuAvailable = true;
            mpuAddress = candidates[i];
            break;
        }
    }
    if (!mpuAvailable) {
        // Recover I2C/OLED if a bad MPU transaction left the bus in a weird state.
        Wire.begin();
        Wire.setClock(100000);
        Wire.setTimeOut(20);
        u8g2.begin();
        u8g2.setContrast(255);
        return;
    }

    mpu.setGyroRange(MPU6050_RANGE_250_DEG);
    mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);
    mpuHeadingDeg = 0.0f;
    mpuLastSampleUs = micros();
}

static void tickMPU6050() {
    if (!mpuAvailable) return;

    uint32_t nowUs = micros();
    if (mpuLastSampleUs == 0) {
        mpuLastSampleUs = nowUs;
        return;
    }
    float dt = (nowUs - mpuLastSampleUs) / 1000000.0f;
    if (dt < 0.005f) return; // ~200 Hz max read rate
    mpuLastSampleUs = nowUs;

    sensors_event_t accel;
    sensors_event_t gyro;
    sensors_event_t temp;
    mpu.getEvent(&accel, &gyro, &temp);

    float zDps = gyro.gyro.z * 57.2957795f; // rad/s -> deg/s
    if (fabsf(zDps) < 0.8f) zDps = 0.0f; // basic noise deadband

    mpuHeadingDeg += zDps * dt;
    while (mpuHeadingDeg >= 360.0f) mpuHeadingDeg -= 360.0f;
    while (mpuHeadingDeg < 0.0f)    mpuHeadingDeg += 360.0f;
}

void setup() {
    Serial.begin(115200);
    Wire.begin();
    Wire.setClock(100000);
    Wire.setTimeOut(20);

    setupScreen();
    delay(120);
    bootStatus("Setting up inputs", 2, 7);
    setupInputs();
    bootStatus("Loading settings", 3, 7);
    prefs.begin("voidOS", false);
    loadProfiles();
    armed = false;
    bootStatus("Initializing MPU6050", 4, 7);
    setupMPU6050();

    bootStatus("Starting WiFi services", 5, 7);
    bootStatus("Starting ESP-NOW", 6, 7);
    nowInit();
    bootStatus("Boot complete", 7, 7);

    packet.throttle  = 1500;
    packet.steering  = 1500;
    packet.seq       = 0;
    refreshPacketState(false);

    currentPage  = hudPage;
    previousPage = nullptr;
    bootMillis = millis();
    lastInputActivityMs = millis();
    displaySleeping = false;
    finishBootScreen();
}

void loop() {
    inputsTick();
    if (consumeUserActivity()) {
        lastInputActivityMs = millis();
        if (displaySleeping) {
            u8g2.setPowerSave(0);
            displaySleeping = false;
        }
    }

    if (!displaySleeping && (millis() - lastInputActivityMs >= screenSleepDelayMs)) {
        u8g2.setPowerSave(1);
        displaySleeping = true;
    }

    tickMPU6050();
    hudTick++;

    nowProcessTelemetry();
    nowApplyTelemetryTimeout(1500);
    if (tele.connected && tele.speedKmh > statsTopSpeedKmh) {
        statsTopSpeedKmh = tele.speedKmh;
    }

    static uint32_t lastStatsPersistMs = 0;
    uint32_t nowMs = millis();
    if (nowMs - lastStatsPersistMs >= 30000UL) {
        uint32_t sessionSec = (nowMs - bootMillis) / 1000UL;
        statsRunSeconds += sessionSec;
        bootMillis = nowMs;
        saveProfiles();
        lastStatsPersistMs = nowMs;
    }

    every(UpdateScreenInterval) {
        if (!displaySleeping) {
            if (previousPage != currentPage) {
                currentPage->init();
                previousPage = currentPage;
            }

            u8g2.clearBuffer();
            currentPage->loop();
            u8g2.sendBuffer();
        }
    }

    // Send RC packet at fixed rate
    static uint32_t lastRadioSendMs = 0;
    uint32_t radioNow = millis();
    if (radioNow - lastRadioSendMs >= (uint32_t)RadioSendIntervalMs) {
        lastRadioSendMs = radioNow;

        if (currentPage == hudPage) {
            // Normal driving — full packet with live sticks
            refreshPacketState(true);
            packet.seq++;
            nowSendPacket((const uint8_t*)&packet, sizeof(packet));

        } else if (calibratePage->calTestActive) {
            // Calibration test — neutral throttle, live steer, armed so RX moves servo
            RCPacket testPkt  = {};
            testPkt.version   = RC_PROTOCOL_VERSION;
            testPkt.seq       = packet.seq++;
            testPkt.throttle  = 1500;
            testPkt.steering  = calibratePage->calTestSteerPwm;
            testPkt.driveMode = currentMode;
            testPkt.armed     = 1;
            nowSendPacket((const uint8_t*)&testPkt, sizeof(testPkt));
        }
    }
}