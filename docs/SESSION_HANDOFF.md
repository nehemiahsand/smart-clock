# Session Handoff

Last updated: 2026-10-18 (America/Chicago)

## Resume here

- Repository: `https://github.com/nehemiahsand/smart-clock`
- Branch: `master`
- Firmware commit at handoff: `cbe2ec6`
- Board: DOIT ESP32 DEVKIT V1
- USB port used: `/dev/ttyUSB0`
- ESP32 MAC: `d4:e9:f4:e5:e6:0c`
- Local source path used: `/home/nehemiah-sanders/Downloads/smart-clock`

Read this file, `PROJECT_TRACKER.md`, `HARDWARE_TEST_CHECKLIST.md`, and
`PWA_HANDOFF.md` before changing behavior.

## Live device state at handoff

The last serial `status` response reported:

- Valid NTP time and `controlMode: smart`
- Wi-Fi connected; MQTT disconnected because only Wi-Fi secrets are configured
- Physical/logical master OFF
- Sharp alarm gate ON
- Sharp alarm output ON
- Sharp controller idle and not ringing
- One physical, temporary, one-time alarm: ID 4 at 6:30 AM on October 19, 2026
- No timers

The live state is authoritative. Query `status` again before relying on this
snapshot because physical controls may have changed it afterward.

## Implemented behavior

- Physical ON/OFF is the logical smart master; it does not directly mirror the
  Sharp alarm switch during normal smart mode.
- Smart-master OFF suspends smart alarms and timers, but the one physical manual
  alarm remains eligible.
- Physical SNOOZE click toggles the global Sharp alarm gate. During ringing it
  turns the gate/output OFF and completes the current occurrence.
- There is exactly one physical/manual alarm slot. A new physical edit replaces
  it, and the temporary entry deletes itself after firing.
- Pressing ALARM only to view the native alarm creates no edit.
- A changed physical alarm is armed and saved immediately on ALARM release.
- HOUR/MINUTE presses are tracked once initially and then every 500 ms while
  held, matching the observed Sharp native two-per-second auto-repeat.
- Routine alarm/timer selection is programmed incrementally without interrupting
  Sharp power. Power cycling is reserved for startup/time synchronization,
  significant time correction, explicit sync, noon sync, or recovery.
- API version 2 exposes master, gate, output, control mode, inferred ringing,
  schedules, timers, time/network state, and Sharp busy/error state.
- MQTT is TLS-only by contract, HTTP uses a bearer token, and OTA uses a separate
  password. These services remain inactive until secrets are configured.

## Verified on hardware

- Native test suite: 18/18 passed.
- ESP32 release build: 995,765 bytes flash (76.0%), 49,776 bytes RAM (15.2%).
- USB flashing succeeded on the board above.
- A physical alarm rang at 11:42 PM with the logical master OFF.
- Physical SNOOZE stopped it, disabled the gate/output, cleared ringing, and the
  temporary entry deleted itself.
- A held MINUTE test advanced the Sharp from 6:29 to 6:47. Firmware counted 18
  steps and saved 6:47 immediately, proving the hold-tracking correction.
- A later live status query showed the current alarm had been changed to 6:30 AM.

## Important invariants

- Do not restore raw ON/OFF mirroring in smart mode.
- Do not reintroduce the one-minute manual-alarm settlement delay.
- Do not treat a view-only ALARM hold as an edit.
- Do not pass ordinary TIME/HOUR/MINUTE presses through in smart mode except the
  explicit ALARM-held manual-edit workflow.
- Keep Sharp alarm output OFF during automatic programming and restore it only
  after successful programming when the gate and eligible occurrence allow it.
- Keep `include/secrets.h` ignored and never commit or print credentials.
- Firmware cannot make exposed or improperly insulated mains wiring safe.

## Remaining work

1. Complete unchecked items in `HARDWARE_TEST_CHECKLIST.md`, especially relay
   behavior during ESP reset, exact hidden-second sync, rollovers, five-minute
   untouched ringing, recurrence/suppression, fallback, and recovery tests.
2. Configure MQTT TLS credentials/CA, HTTP bearer token, and OTA password in the
   ignored `include/secrets.h`; verify broker hostname/certificate matching.
3. Configure Mosquitto for encrypted MQTT and migrate other ESP clients away
   from plaintext before enabling the smart-mirror backend integration.
4. Hand `PWA_HANDOFF.md` to the smart-mirror/PWA agent. Do not build the PWA in
   this repository.
5. Keep a backup alarm until overnight reliability and remaining electrical
   safety checks are complete.

## Useful commands

```bash
pio test -e native
pio run -e esp32doit-devkit-v1
pio run -e esp32doit-devkit-v1 -t upload --upload-port /dev/ttyUSB0
pio device monitor --port /dev/ttyUSB0 --baud 115200 --filter direct
```

At the serial prompt, use `status` to retrieve authoritative state.
