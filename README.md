# ESP32 Sharp Smart Clock

Firmware for a DOIT ESP32 DEVKIT V1 controlling a converted Sharp alarm clock. The Sharp electronics retain the display, timekeeping, buzzer, and one native alarm; the ESP32 adds accurate time, scheduling, timers, persistence, and smart-mirror integration.

Start with [Session Handoff](docs/SESSION_HANDOFF.md), then see the
[Project Tracker](docs/PROJECT_TRACKER.md), [PWA Handoff](docs/PWA_HANDOFF.md),
and [Hardware Checklist](docs/HARDWARE_TEST_CHECKLIST.md).

## Safety

The clock contains mains voltage near the transformer primary.

- Unplug it before wiring changes.
- Do not probe or modify the mains side.
- Measure only low-voltage secondary/control circuits.
- Inspect the repaired yellow secondary lead before powered testing.
- First test relay reset behavior with relay COM/NC disconnected from Sharp power.
- Firmware drives active-low GPIO 23 HIGH early, but cannot control it before startup code runs.

## Capabilities

- Safe PC817/relay initialization and non-blocking control state machines.
- Debounced momentary controls plus a maintained ON/OFF smart-master switch.
- Independent logical master and persisted Sharp alarm gate.
- Physical SNOOZE click toggles the Sharp gate; there is no hold action.
- Noon time synchronization using NTP and current US Central DST rules.
- Incremental native-alarm programming that never interrupts Sharp power.
- Ten recurring/one-time alarms and five persistent timers.
- Five-minute missed-alarm grace and inferred ringing window.
- Physical alarm edits auto-arm and update the single manual-alarm slot immediately when ALARM is released; held HOUR/MINUTE presses are tracked at the Sharp native two-per-second repeat rate.
- CRC-validated Preferences/NVS storage.
- TLS MQTT, bearer-authenticated HTTP, and password-authenticated OTA.
- Host tests for timing, debounce, scheduling, timer recovery, and DST rules.

## Configuration

```bash
cp include/secrets.example.h include/secrets.h
```

The ignored `include/secrets.h` supplies Wi-Fi, MQTT host/port/user/password, MQTT CA PEM, HTTP bearer token, and OTA password. The broker certificate SAN must match `MQTT_HOST`. MQTT waits for valid time before TLS validation.

The transferred two-value secrets file still permits local testing, but network services remain disabled until their secrets are configured.

## Build and test

```bash
pio test -e native
pio run -e esp32doit-devkit-v1
pio run -e esp32doit-devkit-v1 -t upload
pio device monitor -b 115200
```

After the first USB upload and OTA configuration:

```bash
pio run -e esp32doit-devkit-v1 -t upload --upload-port smart-clock.local
```

Classic Arduino OTA is password-authenticated but not guaranteed to be transport-encrypted. Use it only on the trusted LAN.

## Startup and recovery

1. PC817 outputs start LOW; relay output starts HIGH.
2. Physical ON/OFF initializes the smart master.
3. Wi-Fi/NTP retry without a long startup wait.
4. Scheduling stays disabled until time is valid.
5. Physical controls unlock on valid time; after 30 seconds without valid time,
   standalone Sharp fallback passes ordinary controls through.
6. Startup synchronization power-cycles Sharp, establishes a 12:00 native-alarm baseline, and programs the earliest saved occurrence.
7. Local alarms continue through temporary Wi-Fi/MQTT loss.
8. Master OFF pauses timers and suspends smart alarms while physical manual
   alarms remain eligible.
9. Meaningful time correction triggers safe Sharp resynchronization.

The Sharp alarm output is ON only when the persisted gate is enabled and an
eligible occurrence is programmed. Creating, enabling, or materially editing an
alarm or timer explicitly arms the gate. A SNOOZE click toggles the gate; long
presses have no action. PWA dismissal is separate: it holds Sharp OFF, completes
the current occurrence, programs the next one, then restores the prior gate.

Routine alarm and timer changes hold ALARM and advance HOUR/MINUTE from the
last known native alarm. They never toggle the power relay. Relay power cycling
is reserved for startup, noon/explicit clock sync, significant time correction,
and recovery after an uncertain partial operation.

Only one alarm can be stored in the Sharp controller. If the ESP32 fails, only that programmed occurrence remains.

## Serial diagnostics

- `status`, `alarms`, `timers`, `wifi`, `mqtt`, `sync`
- `test button <time|hour|min|alarm|snooze|onoff>`
- `test relay confirm`

Remote clock reset/sync additionally requires authenticated JSON with `"confirm": true`.

## Limitations

- Ringing is inferred; there is no buzzer-sense input.
- There is no battery-backed RTC.
- Timezone handling uses modern US Central rules, not full historical IANA data.
- HTTP is bearer-authenticated but not encrypted; normal PWA operation should use MQTT TLS through the backend.
- Conditional face/book/task completion is a documented future feature only.
- Hardware behavior remains unverified until the checklist results are supplied.
