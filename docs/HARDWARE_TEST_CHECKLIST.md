# Hardware Test Checklist

Record results in `docs/PROJECT_TRACKER.md`. Software builds do not verify electrical behavior.

## Safety

- [ ] Unplug the Sharp clock before wiring work.
- [ ] Inspect and insulate the repaired yellow transformer-secondary lead.
- [ ] Confirm relay COM/NC interrupts only the low-voltage secondary.
- [ ] Do not probe or alter the mains/primary side.
- [ ] Keep measurements on low-voltage control circuits.

## 1. Relay reset-time safety

Disconnect relay COM/NC from Sharp power while leaving the relay module connected to the ESP supply.

1. Reset the ESP32 ten times.
2. Power-cycle USB ten times.
3. Watch the relay LED and listen for a click.

Pass: Relay 1 never activates during reset/boot.

If it activates, stop. Identify the relay module input circuit before choosing a pull-up. Do not attach an arbitrary resistor where it could feed 5 V into an ESP GPIO.

## 2. One-output test

With Sharp unplugged, use:

```
test button time
test button hour
test button min
test button alarm
test button snooze
test button onoff
```

Pass: Only the requested output activates and every momentary output returns LOW. Repeat through authenticated HTTP/MQTT after network configuration.

## 3. Inputs

Verify clean serial events for TIME, HOUR, MIN, ALARM, and SNOOZE. Verify SNOOZE click and two-second hold. Confirm ON/OFF ON reads LOW and OFF reads HIGH. If polarity is reversed, correct firmware configuration before continuing.

## 4. Relay interruption

After steps 1–3 pass:

1. Unplug Sharp and reconnect relay COM/NC in the verified secondary conductor.
2. Prevent access to mains components before powering.
3. Run `test relay confirm`.

Expected: controls release, power is removed for about 10 seconds, power returns, and Sharp flashes 12:00 AM.

## 5. Native timing

Verify:

- [ ] 250 ms presses are reliable.
- [ ] 150 ms release gaps prevent duplicates.
- [ ] Three-second startup wait is sufficient.
- [ ] TIME/ALARM can be held while HOUR/MIN receive pulses.
- [ ] HOUR cycles correctly through AM/PM.
- [ ] MIN rolls 59 to 00.

Change timing constants only from repeatable observations.

## 6. Exact time sync

Run `sync` after NTP is valid. Compare Sharp with a trusted clock.

- [ ] Final MIN pulse selects the target minute.
- [ ] Final MIN pulse resets hidden seconds.
- [ ] Physical input during programming is rejected/logged.
- [ ] Failure restores power and releases outputs.

Repeat near 12 AM, 12 PM, and 11:59 PM.

## 7. Alarm programming

Create near-future test entries.

- [ ] ALARM + HOUR/MIN programs expected AM/PM.
- [ ] Native indicator enables for the next occurrence.
- [ ] Earlier entries replace later native programming.
- [ ] Master OFF suspends smart alarms/timers.
- [ ] Physical manual alarm operates while master is OFF.
- [ ] Manual edit appears after the one-minute settling window.

## 8. Ringing

Record actual untouched buzzer duration: __________

Record actual native snooze interval: __________

- [ ] State reports inferred ringing.
- [ ] SNOOZE click dismisses.
- [ ] SNOOZE two-second hold snoozes.
- [ ] MQTT/PWA dismiss and snooze work.
- [ ] Untouched inferred ringing clears after five minutes.
- [ ] The following occurrence is programmed.

## 9. Network/recovery

- [ ] MQTT connects only to TLS port 8883.
- [ ] Incorrect CA/hostname is rejected.
- [ ] Retained state arrives after backend reconnect.
- [ ] Wi-Fi or MQTT loss does not stop local alarms.
- [ ] ESP restart restores alarms/timers.
- [ ] Master OFF pauses timer remaining duration.
- [ ] Total power loss waits for valid NTP.
- [ ] Significant time correction triggers safe resync.
- [ ] OTA rejects an incorrect password.

## 10. Noon synchronization

Test once without a noon alarm and once with a conflict.

Pass: sync runs once, an active alarm wins, deferred sync runs afterward, and the next native alarm is restored.

## Sign-off

- Firmware commit: __________
- Tester/date: __________
- Remaining assumptions: __________
