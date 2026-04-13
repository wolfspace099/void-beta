# RC Protocol (ESP-NOW)

Protocol version: `1` (`RC_PROTOCOL_VERSION`)

## TX -> RX command packet (`RCPacket`, packed)

```c
uint8_t  version;        // must be 1
uint8_t  seq;            // increments every send
uint16_t throttle;       // 1000..2000 PWM-us style command
uint16_t steering;       // 1000..2000 PWM-us style command
uint8_t  driveMode;      // 0=ECO,1=Normal,2=Turbo
uint8_t  frontLightCmd;  // 0=off,1=on
uint8_t  rearLightCmd;   // 0=off,1=on
uint8_t  fanPctCmd;      // 0..100
uint8_t  armed;          // 0/1 (currently always 1 on transmitter)
```

## RX -> TX telemetry packet (`TelemetryPacket`, packed)

```c
uint8_t  version;          // must be 1
uint8_t  seq;              // receiver telemetry sequence
uint16_t rpm;              // wheel or motor RPM (your chosen source)
uint16_t speedKmhX100;     // km/h * 100
uint16_t battmV;           // battery in millivolts
uint8_t  battPct;          // 0..100
uint8_t  frontLightState;  // 0/1 actual state
uint8_t  rearLightState;   // 0/1 actual state
uint8_t  fanPctState;      // 0..100 actual state
```

## Receiver formulas

### RPM -> km/h

Use wheel RPM for best speed estimate:

```c
float wheelCircumferenceM = 2.0f * PI * wheelRadiusM;
float speedMps = (wheelRpm / 60.0f) * wheelCircumferenceM;
float speedKmh = speedMps * 3.6f;
uint16_t speedKmhX100 = (uint16_t)constrain((int)(speedKmh * 100.0f), 0, 65535);
```

If your encoder measures motor RPM, convert using gear ratio:

```c
float wheelRpm = motorRpm / gearRatio; // gearRatio = motor_rev / wheel_rev
```

### Voltage divider -> battery %

```c
float adcV = (adcRaw / 4095.0f) * 3.3f;
float battV = adcV * dividerScale; // example dividerScale = (R1+R2)/R2
uint16_t battmV = (uint16_t)(battV * 1000.0f);

// Example Li-ion/LiPo mapping window; tune for your pack chemistry
int pct = map((int)(battV * 100), 300, 420, 0, 100);
uint8_t battPct = (uint8_t)constrain(pct, 0, 100);
```

## Notes

- Keep both sides on identical struct layout/types.
- Both packets are packed to avoid padding overhead.
- Transmitter only accepts telemetry with matching `version`.
