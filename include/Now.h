#pragma once

#include <Arduino.h>

bool nowInit();
void nowProcessTelemetry();
void nowApplyTelemetryTimeout(uint32_t timeoutMs);
bool nowSendPacket(const uint8_t* data, size_t len);

void nowSetReceiverMac(const uint8_t mac[6]);
const uint8_t* nowGetReceiverMac();
