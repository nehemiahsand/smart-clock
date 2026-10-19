# Smart-Mirror/PWA Agent Handoff

This is the contract for a separate agent updating the existing smart-mirror backend and PWA. Do not duplicate the firmware scheduler in the PWA. The ESP32 retained state is authoritative.

## Required UI

The Smart Clock subsection must:

- Show device, Wi-Fi, MQTT, time, and Sharp-controller status.
- Show and control the smart master.
- List/create/edit/enable/disable/delete up to ten alarms.
- List/create/enable/disable/delete up to five timers.
- Show scheduled, suspended, disabled, ringing, fired, and suppressed states.
- Identify recurring, one-time, timer, and physical/manual origins.
- Show the next native occurrence and label ringing as inferred.
- Show/control the Sharp alarm gate separately from the smart master.
- Offer transient dismiss and guarded sync/reset; there is no snooze action.
- Eventually show a physical-looking 3D clock with logical clickable controls.
- Keep raw native-output tests in a separate diagnostics area.

The browser talks to the existing backend. Never expose MQTT credentials or CA paths to browser code.

## Encrypt all MQTT traffic

The current port 1883 listener is authenticated but plaintext. The smart-mirror agent must:

1. Create a private local CA and Mosquitto server certificate.
2. Include SANs `DNS:nehemiah-pi5`, `DNS:nehemiah-pi5.local`, and `DNS:mosquitto`; include a reserved LAN IP if clients use it.
3. Configure a TLS listener, normally 8883, with `cafile`, `certfile`, `keyfile`, `allow_anonymous false`, and the existing password file.
4. Mount certificates/keys read-only and expose 8883 in Docker.
5. Change the backend URL to `mqtts://mosquitto:8883`.
6. Pass the private CA to the backend MQTT client and retain hostname verification.
7. Keep username/password authentication in addition to TLS.
8. Close/firewall 1883 after every client is migrated.
9. Migrate the existing ESP32 console to `WiFiClientSecure`, CA validation, and 8883.
10. Never use an insecure TLS mode or disable hostname verification.

The clock stores the CA PEM in ignored `include/secrets.h`.

## Topics

Device ID is `smart-clock`.

- Retained status: `smartmirror/esp32/smart-clock/status`
- Events: `smartmirror/esp32/smart-clock/event`
- Retained state: `smartmirror/esp32/smart-clock/state`
- Commands: `smartmirror/esp32/smart-clock/command`
- Responses: `smartmirror/esp32/smart-clock/response`

Subscribe to status/event/state/response. Publish only commands. The firmware retains state and caches the eight most recent request IDs/responses to prevent duplicate creates during retries.

## Command envelope

```json
{"requestId":"clock-unique-id","action":"timer.create","minutes":20}
```

Success:

```json
{"ok":true,"requestId":"clock-unique-id"}
```

Failure:

```json
{"ok":false,"requestId":"clock-unique-id","error":"busy"}
```

Treat `busy` as a visible rejection; do not queue it.

## Commands

Master:

```json
{"requestId":"...","action":"master.set","enabled":false}
```

Recurring alarm; weekday bits are Sunday bit 0 through Saturday bit 6. Monday-Friday is 62.

```json
{"requestId":"...","action":"alarm.upsert","enabled":true,"recurring":true,"hour":7,"minute":0,"weekdayMask":62}
```

One-time alarm uses epoch seconds and local hour/minute. Include `id` to replace an existing entry.

```json
{"requestId":"...","action":"alarm.upsert","id":12,"enabled":true,"recurring":false,"hour":8,"minute":30,"epoch":1792157400}
```

Enable/delete alarm:

```json
{"requestId":"...","action":"alarm.set_enabled","id":12,"enabled":false}
{"requestId":"...","action":"alarm.delete","id":12}
```

Timer range is 1–1439 minutes:

```json
{"requestId":"...","action":"timer.create","minutes":20}
{"requestId":"...","action":"timer.set_enabled","id":22,"enabled":false}
{"requestId":"...","action":"timer.delete","id":22}
```

Active alarm:

```json
{"requestId":"...","action":"alarm.dismiss"}
```

Global Sharp alarm gate:

```json
{"requestId":"...","action":"sharp_alarm.set_enabled","enabled":false}
```

Enabling without any enabled alarm/timer returns `no_eligible_occurrence`.
Creating, enabling, or materially editing an enabled alarm/timer arms the gate.

Guarded operations require a UI confirmation:

```json
{"requestId":"...","action":"clock.sync","confirm":true}
{"requestId":"...","action":"clock.reset","confirm":true}
```

Logical 3D control:

```json
{"requestId":"...","action":"button","button":"snooze","gesture":"click"}
```

Logical mappings:

- ON/OFF toggles smart master.
- SNOOZE click toggles the global Sharp alarm gate.
- SNOOZE hold and ordinary TIME/HOUR/MIN/ALARM virtual taps are rejected.

Diagnostic-only raw tap:

```json
{"requestId":"...","action":"diagnostic.button","button":"hour","confirm":true}
```

## Retained state schema

Top-level fields:

- `apiVersion` (2), `deviceId`
- `masterEnabled`
- `sharpAlarmGateEnabled`, `sharpAlarmOutputEnabled`
- `controlMode`: `smart` or `manual_fallback`
- `timeValid`, `timeStatus`, `epoch`
- `wifiConnected`, `mqttConnected`
- `sharpBusy` and optional `lastSharpError`
- `ringing`, `ringingInferred`
- Optional `next {id,type,epoch}`
- `alarms[]`, `timers[]`

Alarm fields:

- `id`, `enabled`, `recurring`, `temporary`
- `hour`, `minute`, `weekdayMask`, `epoch`
- `origin`, `state`

Timer fields:

- `id`, `enabled`, `targetEpoch`
- `pausedRemainingSeconds`, `state`

Use retained state as the reconnect snapshot. Apply events for immediate feedback, then reconcile with the next state.

## HTTP diagnostics

- `GET http://smart-clock.local/api/v2/health`
- `GET http://smart-clock.local/api/v2/state`
- `POST http://smart-clock.local/api/v2/command`

All require `Authorization: Bearer <HTTP_BEARER_TOKEN>`. POST uses the same
JSON commands. Legacy `/api/v1` aliases remain during migration. HTTP is not
encrypted; keep it on the trusted LAN and prefer MQTT TLS.

## Backend changes

The current generic ESP32 service only consumes event/status topics. Add a dedicated clock service that:

- Subscribes to clock status, state, event, and response topics.
- Publishes correlated commands and times them out.
- Relays state through the backend's existing real-time browser mechanism.
- Validates PWA input before publishing.
- Does not keep a competing schedule database as source of truth.
- Reports offline/busy/error states without pretending commands succeeded.
- Adds Jest tests for validation, correlation, timeout, retained reconnect, TLS configuration, and authorization.

## UI rules and limitations

- Render times in `America/Chicago`.
- Show weekday chips and physical/manual badges.
- Show suspended entries and paused timer duration while master is off.
- Treat `alarm.suppressed`, `timer.suppressed`, and
  `sharp_alarm.gate_changed` as immediate events, then reconcile retained state.
- Disable/reject controls while `sharpBusy` is true.
- Keep disabled one-time alarms visible.
- Do not expect custom text on the Sharp display.
- Only one native alarm exists; if the ESP fails, only that programmed occurrence remains.
- No RTC or buzzer feedback exists.
- Manual alarm inference assumes every physical edit was observed.
- Nonexistent spring DST times are skipped; repeated fall times fire once.
