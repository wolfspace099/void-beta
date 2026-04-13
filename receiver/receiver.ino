// =============================================================================
//  RC Receiver — Abyss carrier board (ESP32)
//  Receives RCPacket via ESP-NOW from the TX controller.
//  Outputs:
//    - Engine PWM  → ESC  (ESP32Servo, throttle 1000-2000 µs)
//    - Servo PWM   → steering servo (1200-1700-2200 µs)
//    - LED1 / LED2 → front / rear lights via MOSFET
//    - Fan         → fan MOSFET (0-100 % → 0-255 PWM)
//  Sends TelemetryPacket back to TX every TELE_INTERVAL_MS.
//
//  Wire protocol version must match TX: RC_PROTOCOL_VERSION 1
// =============================================================================

#include <Arduino.h>
#include <ESP32Servo.h>
#include <esp_now.h>
#include <WiFi.h>

// ---------------------------------------------------------------------------
// Protocol (keep in sync with TX Globals.h)
// ---------------------------------------------------------------------------
#define RC_PROTOCOL_VERSION 1

typedef struct __attribute__((packed)) {
    uint8_t  version;
    uint8_t  seq;
    uint16_t throttle;       // µs  1000-2000
    uint16_t steering;       // µs  1200-1700-2200
    uint8_t  driveMode;      // 0=ECO 1=Normal 2=Turbo
    uint8_t  frontLightCmd;  // 0=off 1=on
    uint8_t  rearLightCmd;   // 0=off 1=on
    uint8_t  fanPctCmd;      // 0-100
    uint8_t  armed;          // 0/1
} RCPacket;

typedef struct __attribute__((packed)) {
    uint8_t  version;
    uint8_t  seq;
    uint16_t rpm;
    uint16_t speedKmhX100;   // km/h * 100
    uint16_t battmV;
    uint8_t  battPct;
    uint8_t  frontLightState;
    uint8_t  rearLightState;
    uint8_t  fanPctState;
} TelemetryPacket;

// ---------------------------------------------------------------------------
// Pin assignments — Abyss Rev2, verified from J3 schematic
//
// XIAO ESP32C3 left edge (J3, pins 1→7 top→bottom):
//   Pin 1  D0  GPIO2  → BAT_READ (ADC)
//   Pin 2  D1  GPIO3  → NC
//   Pin 3  D2  GPIO4  → Encoder (unused on RX)
//   Pin 4  D3  GPIO5  → NC
//   Pin 5  D4  GPIO6  → SDA (unused on RX)
//   Pin 6  D5  GPIO7  → SCL (unused on RX)
//   Pin 7  D6  GPIO21 → Engine_PWM → ESC
//
// XIAO ESP32C3 right edge (Conn_01x07, pins 1→7 bottom→top):
//   Pin 1  D10 GPIO10 → Servo
//   Pin 2  D9  GPIO9  → Fancntrl  → Q4 → fan MOSFET
//   Pin 3  D8  GPIO8  → LED2cntrl → Q3 → rear light MOSFET
//   Pin 4  D7  GPIO20 → LED1cntrl → Q2 → front light MOSFET
//   Pin 5-6    NC
//   Pin 7      +5V
// ---------------------------------------------------------------------------
#define PIN_ENGINE_PWM  21    // D6  → ESC signal
#define PIN_SERVO       10    // D10 → steering servo signal

#define PIN_LED1_CTRL   20    // D7  → Q2 front light MOSFET
#define PIN_LED2_CTRL    8    // D8  → Q3 rear light MOSFET
#define PIN_FAN_CTRL     9    // D9  → Q4 fan MOSFET

// BAT_READ: R1=300k, R2=100k → Vbat = Vadc * 4.0
#define PIN_BAT_READ     2    // D0  → ADC
#define BAT_DIVIDER_RATIO  4.0f

// ---------------------------------------------------------------------------
// Failsafe / timing
// ---------------------------------------------------------------------------
#define FAILSAFE_TIMEOUT_MS   500   // no packet → neutral outputs
#define TELE_INTERVAL_MS       50   // telemetry send rate (~20 Hz)
#define BATT_SAMPLE_INTERVAL  1000  // battery ADC sample rate

// Safe neutral PWM values
#define THROTTLE_NEUTRAL  1500
#define STEER_NEUTRAL     1700

// ---------------------------------------------------------------------------
// Globals
// ---------------------------------------------------------------------------
static Servo escServo;
static Servo steerServo;

// Last received packet (written in ESP-NOW callback, read in loop)
static portMUX_TYPE  pktMux       = portMUX_INITIALIZER_UNLOCKED;
static volatile bool pktPending   = false;
static RCPacket      pendingPkt   = {};
static RCPacket      activePkt    = {};   // last applied packet

static uint32_t lastPktMs         = 0;
static bool     failsafeActive    = true;

// Telemetry state
static uint8_t  teleSeq           = 0;
static uint32_t lastTeleMs        = 0;
static uint32_t lastBattMs        = 0;
static uint16_t battmV            = 0;
static uint8_t  battPct           = 0;

// TX MAC — filled from first received packet's sender address
static uint8_t txMac[6]          = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static bool    txMacKnown         = false;
static esp_now_peer_info_t txPeer = {};

// Fan PWM — core v3+ uses ledcAttach(pin, freq, bits), ledcWrite(pin, duty)
#define FAN_PWM_FREQ     20000   // 20 kHz — above audible range
#define FAN_PWM_BITS     8

// ---------------------------------------------------------------------------
// Battery ADC → mV and percent
// ---------------------------------------------------------------------------
static void readBattery() {
    // ESP32C3 has factory ADC calibration — analogReadMilliVolts() applies it automatically.
    // Average 8 samples to reduce noise.
    uint32_t sum = 0;
    for (int i = 0; i < 8; i++) {
        sum += analogReadMilliVolts(PIN_BAT_READ);
        delayMicroseconds(200);
    }
    float vAdcMv = sum / 8.0f;
    float vBatMv = vAdcMv * BAT_DIVIDER_RATIO;
    battmV = (uint16_t)constrain(vBatMv, 0, 65535);

    // Configurable cell count — change for 3S builds
    const int   BATT_CELL_COUNT = 2;
    const float V_CELL_MIN  = 3.3f;   // per-cell cutoff
    const float V_CELL_MAX  = 4.2f;   // per-cell full
    float vMin = V_CELL_MIN * BATT_CELL_COUNT * 1000.0f;
    float vMax = V_CELL_MAX * BATT_CELL_COUNT * 1000.0f;
    float pct  = (vBatMv - vMin) / (vMax - vMin) * 100.0f;
    battPct = (uint8_t)constrain((int)pct, 0, 100);
}

// ---------------------------------------------------------------------------
// Apply failsafe: neutral throttle/steer, lights/fan off
// ---------------------------------------------------------------------------
static void applyFailsafe() {
    escServo.writeMicroseconds(THROTTLE_NEUTRAL);
    steerServo.writeMicroseconds(STEER_NEUTRAL);
    digitalWrite(PIN_LED1_CTRL, LOW);
    digitalWrite(PIN_LED2_CTRL, LOW);
    ledcWrite(PIN_FAN_CTRL, 0);
    failsafeActive = true;
    Serial.println("[RX] Failsafe active");
}

// ---------------------------------------------------------------------------
// Apply a valid received packet to all outputs
// ---------------------------------------------------------------------------
static void applyPacket(const RCPacket& p) {
    if (!p.armed) {
        // Disarmed: hold neutral, keep lights/fan from cmd
        escServo.writeMicroseconds(THROTTLE_NEUTRAL);
        steerServo.writeMicroseconds(STEER_NEUTRAL);
    } else {
        escServo.writeMicroseconds(constrain((int)p.throttle, 1000, 2000));
        steerServo.writeMicroseconds(constrain((int)p.steering, 1000, 2200));
    }

    // Lights — direct MOSFET drive
    digitalWrite(PIN_LED1_CTRL, p.frontLightCmd ? HIGH : LOW);
    digitalWrite(PIN_LED2_CTRL, p.rearLightCmd  ? HIGH : LOW);

    // Fan — map 0-100% to 0-255
    uint8_t fanDuty = (uint8_t)map(constrain((int)p.fanPctCmd, 0, 100), 0, 100, 0, 255);
    ledcWrite(PIN_FAN_CTRL, fanDuty);

    failsafeActive = false;
}

// ---------------------------------------------------------------------------
// Send telemetry back to TX
// ---------------------------------------------------------------------------
static void sendTelemetry() {
    if (!txMacKnown) return;

    TelemetryPacket t = {};
    t.version          = RC_PROTOCOL_VERSION;
    t.seq              = teleSeq++;
    t.rpm              = 0;          // no RPM sensor wired; extend here if needed
    t.speedKmhX100     = 0;          // no wheel encoder; extend here if needed
    t.battmV           = battmV;
    t.battPct          = battPct;
    t.frontLightState  = digitalRead(PIN_LED1_CTRL);
    t.rearLightState   = digitalRead(PIN_LED2_CTRL);
    t.fanPctState      = activePkt.fanPctCmd;

    esp_now_send(txMac, (const uint8_t*)&t, sizeof(t));
}

// ---------------------------------------------------------------------------
// ESP-NOW receive callback (runs in WiFi task context)
// ---------------------------------------------------------------------------
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
static void onReceive(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    const uint8_t* senderMac = info->src_addr;
#else
static void onReceive(const uint8_t* senderMac, const uint8_t* data, int len) {
#endif
    if (len != (int)sizeof(RCPacket)) return;

    const RCPacket* p = reinterpret_cast<const RCPacket*>(data);
    if (p->version != RC_PROTOCOL_VERSION) return;

    // Learn TX MAC from first valid packet so we can reply
    if (!txMacKnown) {
        memcpy(txMac, senderMac, 6);
        memset(&txPeer, 0, sizeof(txPeer));
        memcpy(txPeer.peer_addr, txMac, 6);
        txPeer.channel = 0;
        txPeer.encrypt = false;
        esp_now_add_peer(&txPeer);
        txMacKnown = true;
        Serial.printf("[RX] TX MAC learned: %02X:%02X:%02X:%02X:%02X:%02X\n",
            txMac[0],txMac[1],txMac[2],txMac[3],txMac[4],txMac[5]);
    }

    portENTER_CRITICAL_ISR(&pktMux);
    memcpy(&pendingPkt, p, sizeof(RCPacket));
    pktPending = true;
    portEXIT_CRITICAL_ISR(&pktMux);
}

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
static void onSent(const wifi_tx_info_t*, esp_now_send_status_t) {}
#else
static void onSent(const uint8_t*, esp_now_send_status_t) {}
#endif

// ---------------------------------------------------------------------------
// setup
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    Serial.println("[RX] Booting...");

    // Output pins
    pinMode(PIN_LED1_CTRL, OUTPUT);
    pinMode(PIN_LED2_CTRL, OUTPUT);
    digitalWrite(PIN_LED1_CTRL, LOW);
    digitalWrite(PIN_LED2_CTRL, LOW);

    // Fan PWM — ESP32 Arduino core v3+ API: ledcAttach(pin, freq, bits)
    ledcAttach(PIN_FAN_CTRL, FAN_PWM_FREQ, FAN_PWM_BITS);
    ledcWrite(PIN_FAN_CTRL, 0);

    // ADC for battery
    analogReadResolution(12);
    pinMode(PIN_BAT_READ, INPUT);

    // Servos — ESP32Servo auto-allocates timer channels
    // Allow full range needed: engine 1000-2000, steer 1000-2200
    ESP32PWM::allocateTimer(1);
    ESP32PWM::allocateTimer(2);
    escServo.setPeriodHertz(50);
    steerServo.setPeriodHertz(50);
    escServo.attach(PIN_ENGINE_PWM, 1000, 2000);
    steerServo.attach(PIN_SERVO,    1000, 2200);

    // Start in failsafe
    applyFailsafe();

    // Initial battery read
    readBattery();

    // ESP-NOW
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    if (esp_now_init() != ESP_OK) {
        Serial.println("[RX] ESP-NOW init FAILED");
        while (true) { delay(1000); }
    }
    esp_now_register_recv_cb(onReceive);
    esp_now_register_send_cb(onSent);

    Serial.println("[RX] Ready. Waiting for TX...");
    Serial.printf("[RX] My MAC: %s\n", WiFi.macAddress().c_str());
}

// ---------------------------------------------------------------------------
// loop
// ---------------------------------------------------------------------------
void loop() {
    uint32_t now = millis();

    // --- Consume pending packet ---
    if (pktPending) {
        RCPacket pkt;
        portENTER_CRITICAL(&pktMux);
        memcpy(&pkt, &pendingPkt, sizeof(RCPacket));
        pktPending = false;
        portEXIT_CRITICAL(&pktMux);

        applyPacket(pkt);
        activePkt  = pkt;
        lastPktMs  = now;

        if (failsafeActive) {
            Serial.println("[RX] Link established, leaving failsafe");
        }
    }

    // --- Failsafe watchdog ---
    if (!failsafeActive && (now - lastPktMs > FAILSAFE_TIMEOUT_MS)) {
        applyFailsafe();
    }

    // --- Battery sample ---
    if (now - lastBattMs >= BATT_SAMPLE_INTERVAL) {
        lastBattMs = now;
        readBattery();
        Serial.printf("[RX] Batt: %u mV  %u%%\n", battmV, battPct);
    }

    // --- Telemetry ---
    if (now - lastTeleMs >= TELE_INTERVAL_MS) {
        lastTeleMs = now;
        sendTelemetry();
    }
}