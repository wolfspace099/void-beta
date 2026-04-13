#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "Globals.h"
#include "Now.h"

static uint8_t receiverMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static esp_now_peer_info_t peerInfo = {};
static bool nowReady = false;

static portMUX_TYPE teleMux = portMUX_INITIALIZER_UNLOCKED;
static volatile bool telePending = false;
static TelemetryPacket telePendingPacket = {0};

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
static void onSent(const wifi_tx_info_t* txInfo, esp_now_send_status_t status) {
    (void)txInfo;
    (void)status;
}
#else
static void onSent(const uint8_t* mac, esp_now_send_status_t status) {
    (void)mac;
    (void)status;
}
#endif

#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
static void onReceive(const esp_now_recv_info_t* recvInfo, const uint8_t* data, int len) {
    (void)recvInfo;
#else
static void onReceive(const uint8_t* mac, const uint8_t* data, int len) {
    (void)mac;
#endif

    if (len != (int)sizeof(TelemetryPacket)) return;

    const TelemetryPacket* in = reinterpret_cast<const TelemetryPacket*>(data);
    if (in->version != RC_PROTOCOL_VERSION) return;

    portENTER_CRITICAL_ISR(&teleMux);
    memcpy(&telePendingPacket, in, sizeof(TelemetryPacket));
    telePending = true;
    portEXIT_CRITICAL_ISR(&teleMux);
}

static bool configurePeer() {
    memset(&peerInfo, 0, sizeof(peerInfo));
    memcpy(peerInfo.peer_addr, receiverMac, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_is_peer_exist(receiverMac)) esp_now_del_peer(receiverMac);
    return esp_now_add_peer(&peerInfo) == ESP_OK;
}

bool nowInit() {
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) return false;

    esp_now_register_send_cb(onSent);
    esp_now_register_recv_cb(onReceive);
    nowReady = configurePeer();
    return nowReady;
}

void nowProcessTelemetry() {
    if (!telePending) return;

    TelemetryPacket pkt;
    portENTER_CRITICAL(&teleMux);
    memcpy(&pkt, &telePendingPacket, sizeof(TelemetryPacket));
    telePending = false;
    portEXIT_CRITICAL(&teleMux);

    telePacketRx = pkt;
    tele.speedKmh = ((float)pkt.speedKmhX100) / 100.0f;
    tele.battPct  = pkt.battPct;
    tele.rpm      = pkt.rpm;
    tele.battmV   = pkt.battmV;
    tele.frontLightState = pkt.frontLightState;
    tele.rearLightState  = pkt.rearLightState;
    tele.fanPctState     = pkt.fanPctState;
    tele.lastSeq         = pkt.seq;
    tele.connected       = true;
    tele.lastRx          = millis();
}

void nowApplyTelemetryTimeout(uint32_t timeoutMs) {
    if (!tele.connected) return;
    if (millis() - tele.lastRx <= timeoutMs) return;

    tele.connected = false;
    tele.speedKmh  = 0;
    tele.rpm       = 0;
}

bool nowSendPacket(const uint8_t* data, size_t len) {
    if (!nowReady) return false;
    if (len == 0 || data == nullptr) return false;
    return esp_now_send(receiverMac, data, len) == ESP_OK;
}

void nowSetReceiverMac(const uint8_t mac[6]) {
    if (!mac) return;
    memcpy(receiverMac, mac, 6);
    if (nowReady) configurePeer();
}

const uint8_t* nowGetReceiverMac() {
    return receiverMac;
}
