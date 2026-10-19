# Smart Clock Project Tracker

This is the implementation source of truth for the ESP32 Sharp smart-clock conversion. Update it whenever a requirement, assumption, milestone, test result, or integration contract changes.

## Project status

- Phase: software implementation complete; hardware and live-network validation pending
- Board/framework: DOIT ESP32 DEVKIT V1, Arduino, PlatformIO `esp32doit-devkit-v1`
- Timezone: `America/Chicago`
- Git strategy: scoped milestone commits
- Broken transformer-secondary wire: reported repaired
- PWA scope: do not modify the smart-mirror app here; produce a handoff for its agent

## Safety invariants

1. Initialize every PC817 output inactive (`LOW`).
2. Drive active-low relay GPIO 23 `HIGH` as early as firmware permits.
3. Release every PC817 before interrupting Sharp power.
4. Never overlap incompatible output commands.
5. Reject physical and remote commands while automatic programming is busy.
6. Require authentication and explicit confirmation for remote relay reset/sync.
7. Never log secrets.
8. Keep mains work unplugged and measurements on the low-voltage side.

## Hardware map

All inputs are provisionally active-low with `INPUT_PULLUP`.

| Control | Input | Output | Input behavior |
| --- | ---: | ---: | --- |
| TIME | 18 | 25 | Momentary |
| HOUR | 19 | 26 | Momentary |
| MIN | 21 | 27 | Momentary |
| ALARM | 22 | 14 | Momentary; held while editing |
| SNOOZE | 16 | 13 | Momentary input; click toggles logical Sharp alarm gate |
| ON/OFF | 17 | 33 | Maintained input; provisionally LOW=logical master ON |

- Relay: GPIO 23, active-low; LOW interrupts Sharp power.
- GPIO 32 remains unused; a future book switch is documentation-only.
- Physical ON/OFF initializes the smart master at boot. A remote override remains authoritative until the switch moves.

## Sharp behavior and timing

- Power restoration resets the displayed time to 12:00 AM.
- Native alarm enable is derived from the persisted Sharp gate plus an eligible
  programmed occurrence; it never directly mirrors the maintained switch.
- HOUR cycles through AM/PM; use individual pulses, never auto-repeat.
- Provisional timing: 250 ms press, 150 ms release, 10 s power-off, 3 s boot.
- Routine alarm selection advances from the tracked native alarm with ALARM held and never interrupts power.
- Successful power-cycle synchronization re-establishes the native alarm baseline at 12:00; a partial programming failure marks it unknown and requests recovery sync.
- Hold TIME or ALARM while pulsing HOUR/MIN.
- Set time one minute early, then pulse MIN at the target boundary; hidden-second reset requires confirmation.
- Daily automatic synchronization is 12:00 PM local; defer it while an alarm is active.

## Alarm requirements

- 10 smart alarm entries; recurring weekday or dated one-time.
- PWA one-time alarms remain stored but disabled after firing.
- There is one physical manual-alarm slot. A new settled edit replaces its temporary one-time entry, which is deleted after use.
- Physical entries target the next occurrence: today if upcoming, otherwise tomorrow.
- A changed manual alarm auto-arms immediately on ALARM release; its temporary entry is committed after one minute, restarted by each additional edit.
- Viewing the alarm without changing HOUR or MINUTE creates no entry.
- Physical origin is reported to the backend.
- Missed-alarm grace: 5 minutes.
- Spring nonexistent times do not fire; fall repeated times fire once.
- Simultaneous due entries share one native ringing occurrence while all are marked fired.
- Continually program the earliest eligible occurrence into the one native Sharp alarm.

## Timer requirements

- Separate capacity: 5 timers.
- Range: 1 minute through 23 hours 59 minutes.
- Persist absolute targets through ESP restart.
- Smart-master off pauses remaining timer durations; resume them when enabled.

## Master and alarm interaction

- Physical ON/OFF controls only the logical smart master.
- Master off suspends all smart alarms/timers without retroactive firing.
- A physical manual alarm may operate while master is off and is the only active occurrence.
- Smart entries remain visible as suspended.
- Physical SNOOZE click toggles the persisted global Sharp alarm gate.
- Creating, enabling, or materially editing an enabled alarm/timer arms the gate.
- Automatic recurrence/selection never changes a gate the user turned off.
- Gate OFF suppresses due alarms/timers without late replay.
- PWA dismissal temporarily forces Sharp OFF, programs the next occurrence, and
  restores the prior gate; it does not change the global gate.
- Long SNOOZE presses have no action.
- Completion conditions are future documentation only.

## Inferred ringing

- No buzzer-sense input is planned.
- Infer ringing at the scheduled time until dismiss, gate-off, or 5 minutes elapse.
- Always label ringing as inferred.

## Time and recovery

- Use NTP with current `America/Chicago` rules.
- Continue on the ESP clock through temporary network outages.
- After total power loss, disable scheduling until NTP validates time.
- After 30 seconds without valid time, enter standalone fallback: pass
  TIME/HOUR/MIN/ALARM, mirror maintained ON/OFF, and let SNOOZE toggle Sharp
  ON/OFF until valid time returns.
- Restore schedules/timers after restart.
- Recalculate after meaningful time corrections and resync Sharp when safe.
- No RTC is planned.

## Networking and security

- DHCP; hostname/device ID `smart-clock`.
- MQTT broker: `nehemiah-pi5`; configured IP or `.local` fallback if needed.
- MQTT is primary; TLS port 8883 with CA/hostname validation plus username/password.
- Topics:
  - `smartmirror/esp32/smart-clock/status`
  - `smartmirror/esp32/smart-clock/event`
  - `smartmirror/esp32/smart-clock/state`
  - `smartmirror/esp32/smart-clock/command`
  - `smartmirror/esp32/smart-clock/response`
- Retain authoritative state and republish after reconnect.
- HTTP provides bearer-authenticated reads and writes.
- OTA uses a separate password. Classic Arduino OTA authentication is not transport encryption.
- All credentials and CA material remain in ignored local configuration.

## Interfaces and diagnostics

- Physical, serial, MQTT, HTTP, and automation share one command engine.
- Busy commands are rejected, not queued.
- Logical virtual ON/OFF toggles master; logical SNOOZE toggles the Sharp gate.
- Raw native-output diagnostics remain separate.
- Remote relay reset/sync requires literal confirmation.
- Logging: INFO default, configurable DEBUG/WARN/ERROR.
- Commands: `status`, `alarms`, `timers`, `wifi`, `mqtt`, `sync`, `test button <name>`, `test relay confirm`, `log <level>`.

## PWA handoff

Do not implement the PWA here. The handoff must define:

- Alarm/timer CRUD, master state, and suspend/resume behavior.
- Origin labels: PWA, recurring, one-time, timer, and physical.
- Next native alarm, inferred ringing, gate/dismiss, sync/network health, and errors.
- A future 3D clock with logical buttons plus separate raw diagnostics.
- MQTT/HTTP schemas and guarded commands.
- Mosquitto TLS, backend `mqtts://`, trust/hostname validation, and migration of other ESP clients away from plaintext MQTT.

## Milestones

- [ ] 1. Safety, configuration, and host-test foundation
- [ ] 2. Debounced inputs and unified output engine
- [ ] 3. Relay and Sharp programming state machines
- [ ] 4. Wi-Fi and time service
- [ ] 5. Alarm/timer model, persistence, and scheduler
- [ ] 6. Physical manual-alarm import
- [ ] 7. Inferred alarm lifecycle
- [ ] 8. TLS MQTT, authenticated HTTP, and OTA
- [ ] 9. Reliability, docs, PWA handoff, and final verification

## Unresolved hardware checks

- [ ] Verify GPIO 17 LOW means physical ON.
- [ ] Verify relay does not activate during ESP reset/boot.
- [ ] Verify final MIN pulse resets hidden seconds.
- [ ] Confirm press/release/reset/boot timings.
- [ ] Confirm alarm HOUR/MIN rollover.
- [ ] Confirm untouched buzzer stops after about 5 minutes.
- [ ] Confirm broker hostname resolution and TLS certificate name.

Software builds and host tests do not verify electrical behavior.

## Verification log

- 2026-10-16: Preserved transferred prototype built successfully.
- 2026-10-16: Milestone 1 added secrets hygiene, configuration, and native tests.
- 2026-10-16: Native suite passed 13/13 tests covering wrap-safe timing, debounce, scheduling, timer pause/resume, missed-alarm grace, and DST policies.
- 2026-10-16: Final ESP32 release build succeeded: 989,817 bytes flash (75.5%) and 49,752 bytes RAM (15.2%).
- 2026-10-16: `git diff --check` passed.
- 2026-10-18: Logical master, persisted Sharp alarm gate, suppression,
  transient dismissal, API v2, and standalone fallback implemented.
- 2026-10-18: Native suite passed 16/16 tests; ESP32 release build succeeded
  at 993,229 bytes flash (75.8%) and 49,768 bytes RAM (15.2%).
- 2026-10-18: Added non-power-cycling incremental alarm programming, tracked
  native-alarm state, immediate physical-edit arming, and view-only ALARM handling.
- 2026-10-18: Native suite passed 17/17 tests; ESP32 release build succeeded
  at 995,473 bytes flash (75.9%) and 49,776 bytes RAM (15.2%).
- 2026-10-18: Enforced one replaceable physical manual-alarm slot; boot cleanup
  reduced three legacy test entries to the newest entry, ID 3.
- 2026-10-18: Final USB flash succeeded; live API v2 state showed valid NTP
  time, one physical alarm, gate/output enabled, and an idle Sharp controller.
- 2026-10-18: USB flash verified on ESP32 MAC `d4:e9:f4:e5:e6:0c`;
  API v2 boot and 30-second `manual_fallback` transition observed over serial.
- Hardware behavior, MQTT TLS, HTTP, and OTA remain unverified on the actual devices.
