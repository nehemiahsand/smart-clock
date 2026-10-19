#include "smart_clock_app.h"

#include <ArduinoJson.h>
#include <WiFi.h>
#include "app_config.h"
#include "time_utils.h"

SmartClockApp* SmartClockApp::instance_ = nullptr;

namespace {
Schedule::Alarm* findAlarm(PersistedData& data, uint32_t id) {
  for (auto& alarm : data.alarms)
    if (alarm.used && alarm.id == id) return &alarm;
  return nullptr;
}
Schedule::Timer* findTimer(PersistedData& data, uint32_t id) {
  for (auto& timer : data.timers)
    if (timer.used && timer.id == id) return &timer;
  return nullptr;
}
Schedule::Alarm* freeAlarm(PersistedData& data) {
  for (auto& alarm : data.alarms)
    if (!alarm.used) return &alarm;
  return nullptr;
}
Schedule::Alarm* physicalAlarm(PersistedData& data) {
  Schedule::Alarm* newest = nullptr;
  for (auto& alarm : data.alarms) {
    if (!alarm.used || alarm.origin != Schedule::Origin::Physical) continue;
    if (!newest || alarm.id > newest->id) newest = &alarm;
  }
  return newest;
}
bool removeDuplicatePhysicalAlarms(PersistedData& data) {
  Schedule::Alarm* keep = physicalAlarm(data);
  bool changed = false;
  for (auto& alarm : data.alarms) {
    if (!alarm.used || alarm.origin != Schedule::Origin::Physical ||
        &alarm == keep) continue;
    alarm = Schedule::Alarm{};
    changed = true;
  }
  return changed;
}
Schedule::Timer* freeTimer(PersistedData& data) {
  for (auto& timer : data.timers)
    if (!timer.used) return &timer;
  return nullptr;
}
time_t nextLocalOccurrence(time_t now, uint8_t hour, uint8_t minute) {
  tm local{};
  localtime_r(&now, &local);
  local.tm_hour = hour;
  local.tm_min = minute;
  local.tm_sec = 0;
  local.tm_isdst = -1;
  time_t result = mktime(&local);
  if (result <= now) {
    local.tm_mday += 1;
    local.tm_isdst = -1;
    result = mktime(&local);
  }
  return result;
}
ControlId parseControl(const String& value) {
  String name = value;
  name.toLowerCase();
  if (name == "time") return ControlId::Time;
  if (name == "hour") return ControlId::Hour;
  if (name == "min" || name == "minute") return ControlId::Minute;
  if (name == "alarm") return ControlId::Alarm;
  if (name == "snooze") return ControlId::Snooze;
  if (name == "onoff" || name == "on/off") return ControlId::OnOff;
  return ControlId::Count;
}

bool sameOccurrence(const Schedule::Occurrence& left,
                    const Schedule::Occurrence& right) {
  if (left.valid != right.valid) return false;
  if (!left.valid) return true;
  return left.timer == right.timer && left.id == right.id &&
         left.epoch == right.epoch;
}
}

String SmartClockApp::stateThunk() {
  return instance_ ? instance_->buildStateJson() : "{}";
}

bool SmartClockApp::commandThunk(const String& request, String& response) {
  return instance_ && instance_->handleCommand(request, response);
}

void SmartClockApp::begin() {
  instance_ = this;
  sharp_.begin();
  bootMs_ = millis();
  panel_.begin(bootMs_);
  masterEnabled_ = panel_.switchOn();

  Serial.begin(115200);
  Serial.println("\n[info] smart clock starting safely");

  if (!store_.begin()) Serial.printf("[error] %s\n", store_.lastError());
  if (!store_.load(data_))
    Serial.printf("[warn] settings reset: %s\n", store_.lastError());
  if (removeDuplicatePhysicalAlarms(data_)) {
    if (!store_.save(data_))
      Serial.printf("[error] physical alarm cleanup failed: %s\n",
                    store_.lastError());
    else
      Serial.println("[info] removed duplicate physical alarm entries");
  }
  if (!hasEnabledEntries()) data_.sharpAlarmGateEnabled = false;

  time_.begin(bootMs_);
  network_.begin(stateThunk, commandThunk);
  Serial.println("[info] controls locked until valid time or startup timeout");
}

void SmartClockApp::markDirty() {
  if (!store_.save(data_))
    Serial.printf("[error] persistence failed: %s\n", store_.lastError());
  network_.publishState();
}

bool SmartClockApp::hasEnabledEntries() const {
  return Schedule::hasEnabledEntries(
      data_.alarms, Config::MAX_ALARMS, data_.timers, Config::MAX_TIMERS);
}

bool SmartClockApp::hasGateEligibleEntry() const {
  return hasEnabledEntries() || manualEditPending_;
}

bool SmartClockApp::nativeAlarmMatches(uint8_t hour, uint8_t minute) const {
  return nativeAlarmKnown_ && nativeAlarmHour_ == hour &&
         nativeAlarmMinute_ == minute;
}

void SmartClockApp::setSharpOutput(bool enabled) {
  sharpOutputEnabled_ = enabled;
  sharp_.setNativeAlarmEnabled(enabled);
}

void SmartClockApp::clearGateWithoutEntries() {
  if (hasGateEligibleEntry()) return;
  data_.sharpAlarmGateEnabled = false;
  if (!manualFallback_) setSharpOutput(false);
}

void SmartClockApp::enterManualFallback() {
  if (manualFallback_) return;
  manualFallback_ = true;
  fallbackSharpEnabled_ = masterEnabled_;
  setSharpOutput(fallbackSharpEnabled_);
  Serial.println("[warn] entering standalone Sharp fallback mode");
  network_.publishEvent("clock.manual_fallback");
}

void SmartClockApp::exitManualFallback() {
  if (!manualFallback_) return;
  manualFallback_ = false;
  fallbackSharpEnabled_ = false;
  setSharpOutput(false);
  needsClockSync_ = true;
  Serial.println("[info] leaving manual fallback; synchronization required");
  network_.publishEvent("clock.smart_mode");
}

void SmartClockApp::setMaster(bool enabled, time_t now, const char* source) {
  if (masterEnabled_ == enabled) return;
  Schedule::Occurrence previous{};
  if (now > 0) {
    previous = Schedule::nextOccurrence(
        data_.alarms, Config::MAX_ALARMS, data_.timers,
        Config::MAX_TIMERS, masterEnabled_, now);
  }
  if (now > 0 && !enabled) {
    Schedule::suspendTimers(data_.timers, Config::MAX_TIMERS, now);
    for (auto& alarm : data_.alarms)
      if (alarm.used && alarm.origin != Schedule::Origin::Physical &&
          alarm.enabled) alarm.state = Schedule::EntryState::Suspended;
  } else if (now > 0) {
    Schedule::resumeTimers(data_.timers, Config::MAX_TIMERS, now);
    for (auto& alarm : data_.alarms)
      if (alarm.used && alarm.enabled &&
          alarm.state == Schedule::EntryState::Suspended)
        alarm.state = Schedule::EntryState::Scheduled;
  }
  masterEnabled_ = enabled;
  if (!manualFallback_ && now > 0) {
    recomputeNext(now);
    if (!sameOccurrence(previous, next_)) {
      if (sharp_.busy())
        sharp_.cancel("eligible occurrence changed with smart master");
      setSharpOutput(false);
      needsAlarmProgramming_ = next_.valid;
    }
  }
  Serial.printf("[info] smart master %s by %s\n", enabled ? "on" : "off", source);
  markDirty();
}

bool SmartClockApp::setSharpAlarmGate(
    bool enabled, time_t now, const char* source) {
  Schedule::Occurrence eligible{};
  if (now > 0) {
    eligible = Schedule::nextOccurrence(
        data_.alarms, Config::MAX_ALARMS, data_.timers,
        Config::MAX_TIMERS, masterEnabled_, now);
  }
  if (enabled && !manualEditPending_ && !eligible.valid) return false;
  if (data_.sharpAlarmGateEnabled == enabled) return true;

  data_.sharpAlarmGateEnabled = enabled;
  if (!enabled) {
    setSharpOutput(false);
    if (ringing_) {
      completeRinging(now, Schedule::Completion::Fired);
      needsAlarmProgramming_ = true;
      network_.publishEvent("alarm.dismiss");
    }
  } else if (manualEditPending_ && nativeAlarmKnown_) {
    setSharpOutput(true);
  } else if (eligible.valid) {
    next_ = eligible;
    tm alarmLocal{};
    localtime_r(&next_.epoch, &alarmLocal);
    if (nativeAlarmMatches(alarmLocal.tm_hour, alarmLocal.tm_min)) {
      setSharpOutput(true);
    } else {
      setSharpOutput(false);
      needsAlarmProgramming_ = true;
    }
  }

  Serial.printf("[info] Sharp alarm gate %s by %s\n",
                enabled ? "on" : "off", source);
  String payload = String("{\"enabled\":") + (enabled ? "true" : "false") +
                   ",\"source\":\"" + source + "\"}";
  network_.publishEvent("sharp_alarm.gate_changed", payload);
  markDirty();
  return true;
}

void SmartClockApp::handleInput(const InputEvent& event, uint32_t nowMs) {
  if (!controlsUnlocked_) return;
  if (event.control == ControlId::OnOff) {
    if (event.type == InputEventType::SwitchOn ||
        event.type == InputEventType::SwitchOff) {
      const bool enabled = event.type == InputEventType::SwitchOn;
      setMaster(enabled, time_.valid() ? time_.now() : 0, "physical");
      if (manualFallback_) {
        fallbackSharpEnabled_ = enabled;
        setSharpOutput(enabled);
      }
    }
    return;
  }
  if (sharp_.busy()) {
    if (event.type != InputEventType::Released)
      Serial.printf("[warn] ignored physical %s: busy\n", controlName(event.control));
    return;
  }
  if (event.control == ControlId::Snooze) {
    if (event.type == InputEventType::Clicked) {
      if (manualFallback_) {
        fallbackSharpEnabled_ = !fallbackSharpEnabled_;
        setSharpOutput(fallbackSharpEnabled_);
      } else if (!setSharpAlarmGate(
                     !data_.sharpAlarmGateEnabled, time_.now(), "physical")) {
        Serial.println("[warn] Sharp alarm gate cannot turn on: no enabled entry");
      }
    }
    return;
  }

  if (!manualFallback_ && event.control == ControlId::Alarm &&
      event.type == InputEventType::Pressed && !nativeAlarmKnown_) {
    needsClockSync_ = true;
    Serial.println("[warn] manual alarm edit deferred: native alarm unknown");
    return;
  }

  const bool routeToSharp = manualFallback_ ||
      event.control == ControlId::Alarm ||
      (manualEditActive_ &&
       (event.control == ControlId::Hour ||
        event.control == ControlId::Minute));
  if (routeToSharp) {
    if (event.type == InputEventType::Pressed)
      sharp_.setManualOutput(event.control, true);
    else if (event.type == InputEventType::Released)
      sharp_.setManualOutput(event.control, false);
  }

  if (event.control == ControlId::Alarm) {
    if (event.type == InputEventType::Pressed) {
      manualEditActive_ = true;
      manualEditChanged_ = false;
      manualOutputBeforeEdit_ = sharpOutputEnabled_;
      if (nativeAlarmKnown_) {
        manualHour_ = nativeAlarmHour_;
        manualMinute_ = nativeAlarmMinute_;
      } else {
        manualHour_ = 0;
        manualMinute_ = 0;
      }
      if (!manualFallback_) setSharpOutput(false);
    } else if (event.type == InputEventType::Released && manualEditActive_) {
      manualEditActive_ = false;
      if (!manualEditChanged_) {
        if (!manualFallback_) setSharpOutput(manualOutputBeforeEdit_);
        return;
      }

      nativeAlarmKnown_ = true;
      nativeAlarmHour_ = manualHour_;
      nativeAlarmMinute_ = manualMinute_;
      manualEditPending_ = true;
      manualLastEditMs_ = nowMs;
      needsAlarmProgramming_ = false;
      const bool gateChanged = !data_.sharpAlarmGateEnabled;
      data_.sharpAlarmGateEnabled = true;
      if (!manualFallback_) setSharpOutput(true);
      if (gateChanged) {
        network_.publishEvent(
            "sharp_alarm.gate_changed",
            "{\"enabled\":true,\"source\":\"physical-manual\"}");
      }
      markDirty();
      Serial.printf("[info] physical alarm pending %02u:%02u\n",
                    manualHour_, manualMinute_);
    }
  } else if (manualEditActive_ && event.type == InputEventType::Clicked) {
    if (event.control == ControlId::Hour) {
      manualHour_ = static_cast<uint8_t>((manualHour_ + 1) % 24);
      manualEditChanged_ = true;
    } else if (event.control == ControlId::Minute) {
      manualMinute_ = static_cast<uint8_t>((manualMinute_ + 1) % 60);
      manualEditChanged_ = true;
    }
  }
}

void SmartClockApp::handleInputs(uint32_t nowMs) {
  panel_.update(nowMs);
  InputEvent event{};
  while (panel_.pop(event)) handleInput(event, nowMs);
}

bool SmartClockApp::addPhysicalAlarm(time_t now) {
  removeDuplicatePhysicalAlarms(data_);
  Schedule::Alarm* alarm = physicalAlarm(data_);
  if (!alarm) alarm = freeAlarm(data_);
  if (!alarm) return false;
  const uint32_t id = alarm->used ? alarm->id : data_.nextId++;
  *alarm = Schedule::Alarm{};
  alarm->id = id;
  alarm->used = alarm->enabled = alarm->temporary = true;
  alarm->origin = Schedule::Origin::Physical;
  alarm->state = Schedule::EntryState::Scheduled;
  alarm->hour = manualHour_;
  alarm->minute = manualMinute_;
  alarm->oneTimeEpoch = nextLocalOccurrence(now, manualHour_, manualMinute_);

  recomputeNext(now);
  if (next_.valid) {
    tm selected{};
    localtime_r(&next_.epoch, &selected);
    if (nativeAlarmMatches(selected.tm_hour, selected.tm_min)) {
      setSharpOutput(data_.sharpAlarmGateEnabled);
      needsAlarmProgramming_ = false;
    } else {
      setSharpOutput(false);
      needsAlarmProgramming_ = true;
    }
  } else {
    setSharpOutput(false);
    needsAlarmProgramming_ = false;
  }
  markDirty();
  return true;
}

void SmartClockApp::updateManualImport(uint32_t nowMs) {
  if (!manualEditPending_ ||
      !TimeUtils::elapsed(nowMs, manualLastEditMs_, Config::MANUAL_ALARM_SETTLE_MS))
    return;
  if (!time_.valid()) {
    return;
  }
  manualEditPending_ = false;
  if (addPhysicalAlarm(time_.now()))
    Serial.printf("[info] imported physical alarm %02u:%02u\n",
                  manualHour_, manualMinute_);
  else {
    Serial.println("[error] alarm capacity full; physical alarm not imported");
    needsAlarmProgramming_ = true;
  }
}

void SmartClockApp::recomputeNext(time_t now) {
  next_ = Schedule::nextOccurrence(
      data_.alarms, Config::MAX_ALARMS, data_.timers,
      Config::MAX_TIMERS, masterEnabled_, now);
}

bool SmartClockApp::startProgramming(bool forceTimeSync) {
  if (!time_.valid() || sharp_.busy()) return false;
  recomputeNext(time_.now());

  uint8_t alarmHour = 0;
  uint8_t alarmMinute = 0;
  if (next_.valid) {
    tm alarmLocal{};
    localtime_r(&next_.epoch, &alarmLocal);
    alarmHour = static_cast<uint8_t>(alarmLocal.tm_hour);
    alarmMinute = static_cast<uint8_t>(alarmLocal.tm_min);
  }
  const bool alarmOn = next_.valid && data_.sharpAlarmGateEnabled;

  if (!forceTimeSync) {
    if (!next_.valid) {
      setSharpOutput(false);
      needsAlarmProgramming_ = false;
      return true;
    }
    if (!nativeAlarmKnown_) {
      needsAlarmProgramming_ = false;
      needsClockSync_ = true;
      return false;
    }
    if (nativeAlarmMatches(alarmHour, alarmMinute)) {
      setSharpOutput(alarmOn);
      needsAlarmProgramming_ = false;
      return true;
    }

    setSharpOutput(false);
    const auto accepted = sharp_.programAlarm(
        nativeAlarmHour_, nativeAlarmMinute_, alarmHour, alarmMinute,
        alarmOn, millis());
    if (accepted == SharpController::Result::Accepted) {
      pendingAlarmHour_ = alarmHour;
      pendingAlarmMinute_ = alarmMinute;
      needsAlarmProgramming_ = false;
      return true;
    }
    return false;
  }

  int64_t targetMs = ((time_.nowMilliseconds() / 60000) + 1) * 60000;
  tm target{};
  while (true) {
    time_t seconds = static_cast<time_t>(targetMs / 1000);
    localtime_r(&seconds, &target);
    const uint32_t duration =
        Config::RELAY_OFF_MS + Config::SHARP_BOOT_MS +
        TimeUtils::estimateButtonSequenceMs(
            TimeUtils::hourPressesFromMidnight(target.tm_hour),
            TimeUtils::minuteBeforeTarget(target.tm_min),
            Config::MODE_START_MS, Config::BUTTON_PRESS_MS,
            Config::BUTTON_RELEASE_MS, Config::MODE_END_MS);
    if (targetMs - time_.nowMilliseconds() >
        static_cast<int64_t>(duration + 1000)) break;
    targetMs += 60000;
  }

  setSharpOutput(false);
  const auto accepted = sharp_.synchronize(
      target.tm_hour, target.tm_min, targetMs, next_.valid,
      alarmHour, alarmMinute, alarmOn, millis());
  if (accepted == SharpController::Result::Accepted) {
    pendingAlarmHour_ = next_.valid ? alarmHour : 0;
    pendingAlarmMinute_ = next_.valid ? alarmMinute : 0;
    nativeAlarmKnown_ = false;
    needsClockSync_ = false;
    needsAlarmProgramming_ = false;
    return true;
  }
  return false;
}

void SmartClockApp::startRinging(time_t now, uint32_t nowMs) {
  ringing_ = true;
  ringStartedMs_ = nowMs;
  for (auto& alarm : data_.alarms) {
    const Schedule::Occurrence due = Schedule::dueOccurrence(
        &alarm, 1, nullptr, 0, masterEnabled_, now,
        Config::MISSED_GRACE_SECONDS);
    if (due.valid && due.epoch == next_.epoch)
      alarm.state = Schedule::EntryState::Ringing;
  }
  for (auto& timer : data_.timers)
    if (timer.used && timer.enabled && timer.targetEpoch == next_.epoch)
      timer.state = Schedule::EntryState::Ringing;
  Serial.println("[info] inferred alarm ringing");
  network_.publishEvent("alarm.ringing");
  markDirty();
}

void SmartClockApp::completeRinging(
    time_t, Schedule::Completion completion) {
  if (!ringing_) return;
  ringing_ = false;
  for (auto& alarm : data_.alarms) {
    if (!alarm.used || alarm.state != Schedule::EntryState::Ringing) continue;
    Schedule::completeAlarm(alarm, next_.epoch, completion);
  }
  for (auto& timer : data_.timers) {
    if (!timer.used || timer.state != Schedule::EntryState::Ringing) continue;
    Schedule::completeTimer(timer, completion);
  }
  clearGateWithoutEntries();
}

void SmartClockApp::beginDismiss(time_t now) {
  if (!ringing_) return;
  completeRinging(now, Schedule::Completion::Fired);
  setSharpOutput(false);
  dismissPhase_ = DismissPhase::HoldOff;
  dismissStartedMs_ = millis();
  needsAlarmProgramming_ = true;
  network_.publishEvent("alarm.dismiss");
  markDirty();
}

void SmartClockApp::suppressDue(
    time_t now, const Schedule::Occurrence& occurrence) {
  for (auto& alarm : data_.alarms) {
    const Schedule::Occurrence due = Schedule::dueOccurrence(
        &alarm, 1, nullptr, 0, masterEnabled_, now,
        INT32_MAX);
    if (!due.valid || due.epoch != occurrence.epoch) continue;
    const uint32_t id = alarm.id;
    Schedule::completeAlarm(
        alarm, occurrence.epoch, Schedule::Completion::Suppressed);
    network_.publishEvent(
        "alarm.suppressed",
        String("{\"id\":") + id + ",\"epoch\":" +
        static_cast<int64_t>(occurrence.epoch) + "}");
  }
  if (masterEnabled_) {
    for (auto& timer : data_.timers) {
      if (!timer.used || !timer.enabled ||
          timer.targetEpoch != occurrence.epoch) continue;
      const uint32_t id = timer.id;
      Schedule::completeTimer(timer, Schedule::Completion::Suppressed);
      network_.publishEvent(
          "timer.suppressed",
          String("{\"id\":") + id + ",\"epoch\":" +
          static_cast<int64_t>(occurrence.epoch) + "}");
    }
  }
  clearGateWithoutEntries();
  markDirty();
  needsAlarmProgramming_ = true;
}

void SmartClockApp::updateSchedule(uint32_t nowMs) {
  if (!time_.valid()) return;
  const time_t now = time_.now();

  if (time_.consumeSignificantStep()) needsClockSync_ = true;

  if (sharpWasBusy_ && !sharp_.busy()) {
    const SharpController::Job finishedJob = sharp_.lastFinishedJob();
    if (!sharp_.lastSucceeded()) {
      Serial.printf("[error] Sharp operation failed: %s\n", sharp_.lastError());
      if (finishedJob == SharpController::Job::Synchronize ||
          finishedJob == SharpController::Job::ProgramAlarm ||
          finishedJob == SharpController::Job::Reset) {
        nativeAlarmKnown_ = false;
        setSharpOutput(false);
        needsAlarmProgramming_ = false;
        needsClockSync_ = true;
      }
      if (dismissPhase_ == DismissPhase::Programming) {
        dismissPhase_ = DismissPhase::HoldOff;
        dismissStartedMs_ = nowMs - Config::ALARM_DISMISS_OFF_MS;
      }
    } else {
      sharpOutputEnabled_ = sharp_.nativeAlarmEnabled();
      if (finishedJob == SharpController::Job::Synchronize ||
          finishedJob == SharpController::Job::ProgramAlarm) {
        nativeAlarmKnown_ = true;
        nativeAlarmHour_ = pendingAlarmHour_;
        nativeAlarmMinute_ = pendingAlarmMinute_;
      } else if (finishedJob == SharpController::Job::Reset) {
        nativeAlarmKnown_ = true;
        nativeAlarmHour_ = 0;
        nativeAlarmMinute_ = 0;
      }
      if (dismissPhase_ == DismissPhase::Programming)
        dismissPhase_ = DismissPhase::None;
      if (finishedJob == SharpController::Job::Synchronize) {
        Serial.println("[info] Sharp clock synchronization completed");
        network_.publishEvent("clock.synchronized");
      } else if (finishedJob == SharpController::Job::ProgramAlarm) {
        Serial.println("[info] Sharp alarm programming completed");
        network_.publishEvent("alarm.programmed");
      } else {
        Serial.println("[info] Sharp operation completed");
      }
    }
  }
  sharpWasBusy_ = sharp_.busy();

  if (dismissPhase_ == DismissPhase::HoldOff) {
    setSharpOutput(false);
    if (!TimeUtils::elapsed(
            nowMs, dismissStartedMs_, Config::ALARM_DISMISS_OFF_MS)) {
      return;
    }
    recomputeNext(now);
    if (!next_.valid && !needsClockSync_) {
      clearGateWithoutEntries();
      dismissPhase_ = DismissPhase::None;
      needsAlarmProgramming_ = false;
      markDirty();
      return;
    }
    if (startProgramming(needsClockSync_)) {
      if (sharp_.busy()) {
        dismissPhase_ = DismissPhase::Programming;
        sharpWasBusy_ = true;
      } else {
        dismissPhase_ = DismissPhase::None;
      }
    }
    return;
  }
  if (dismissPhase_ == DismissPhase::Programming || sharp_.busy()) return;
  if (manualEditActive_ || manualEditPending_) return;

  tm local{};
  localtime_r(&now, &local);
  if (local.tm_hour == Config::DAILY_SYNC_HOUR &&
      local.tm_min == Config::DAILY_SYNC_MINUTE &&
      lastDailySyncYday_ != local.tm_yday) {
    lastDailySyncYday_ = local.tm_yday;
    needsClockSync_ = true;
  }

  if (ringing_) {
    if (TimeUtils::elapsed(nowMs, ringStartedMs_, Config::RING_WINDOW_MS))
      beginDismiss(now);
    return;
  }

  const int32_t grace = data_.sharpAlarmGateEnabled
      ? Config::MISSED_GRACE_SECONDS : INT32_MAX;
  next_ = Schedule::dueOccurrence(
      data_.alarms, Config::MAX_ALARMS, data_.timers, Config::MAX_TIMERS,
      masterEnabled_, now, grace);
  if (next_.valid) {
    if (data_.sharpAlarmGateEnabled)
      startRinging(now, nowMs);
    else
      suppressDue(now, next_);
    return;
  }
  recomputeNext(now);
  clearGateWithoutEntries();

  if (needsClockSync_)
    startProgramming(true);
  else if (needsAlarmProgramming_)
    startProgramming(false);
}

String SmartClockApp::buildStateJson() {
  JsonDocument doc;
  doc["apiVersion"] = 2;
  doc["deviceId"] = Config::DEVICE_ID;
  doc["masterEnabled"] = masterEnabled_;
  doc["sharpAlarmGateEnabled"] = data_.sharpAlarmGateEnabled;
  doc["sharpAlarmOutputEnabled"] = sharpOutputEnabled_;
  doc["controlMode"] = manualFallback_ ? "manual_fallback" : "smart";
  doc["timeValid"] = time_.valid();
  doc["timeStatus"] = time_.status();
  doc["epoch"] = static_cast<int64_t>(time_.now());
  doc["wifiConnected"] = time_.wifiConnected();
  doc["mqttConnected"] = network_.mqttConnected();
  doc["sharpBusy"] = sharp_.busy();
  if (sharp_.lastError()[0]) doc["lastSharpError"] = sharp_.lastError();
  doc["ringing"] = ringing_;
  doc["ringingInferred"] = ringing_;
  if (next_.valid) {
    doc["next"]["id"] = next_.id;
    doc["next"]["type"] = next_.timer ? "timer" : "alarm";
    doc["next"]["epoch"] = static_cast<int64_t>(next_.epoch);
  }
  JsonArray alarms = doc["alarms"].to<JsonArray>();
  for (const auto& alarm : data_.alarms) {
    if (!alarm.used) continue;
    JsonObject item = alarms.add<JsonObject>();
    item["id"] = alarm.id;
    item["enabled"] = alarm.enabled;
    item["recurring"] = alarm.recurring;
    item["temporary"] = alarm.temporary;
    item["hour"] = alarm.hour;
    item["minute"] = alarm.minute;
    item["weekdayMask"] = alarm.weekdayMask;
    item["epoch"] = static_cast<int64_t>(alarm.oneTimeEpoch);
    item["origin"] = Schedule::originName(alarm.origin);
    item["state"] = Schedule::stateName(alarm.state);
  }
  JsonArray timers = doc["timers"].to<JsonArray>();
  for (const auto& timer : data_.timers) {
    if (!timer.used) continue;
    JsonObject item = timers.add<JsonObject>();
    item["id"] = timer.id;
    item["enabled"] = timer.enabled;
    item["targetEpoch"] = static_cast<int64_t>(timer.targetEpoch);
    item["pausedRemainingSeconds"] = timer.pausedRemainingSeconds;
    item["state"] = Schedule::stateName(timer.state);
  }
  String output;
  serializeJson(doc, output);
  return output;
}

bool SmartClockApp::handleCommand(const String& request, String& response) {
  JsonDocument input;
  if (deserializeJson(input, request)) {
    response = "{\"ok\":false,\"error\":\"invalid_json\"}";
    return false;
  }
  const String requestId = input["requestId"] | "";
  if (requestId.length() > 64) {
    response = "{\"ok\":false,\"error\":\"invalid_request_id\"}";
    return false;
  }
  if (requestId.length()) {
    for (uint8_t i = 0; i < 8; ++i) {
      if (recentRequestIds_[i] == requestId) {
        response = recentResponses_[i];
        return response.indexOf("\"ok\":true") >= 0;
      }
    }
  }
  const String action = input["action"] | "";
  if (sharp_.busy()) {
    response = "{\"ok\":false,\"error\":\"busy\"}";
    return false;
  }
  bool ok = true;
  String error;
  const time_t now = time_.now();

  if (action == "master.set") {
    if (!now) { ok = false; error = "time_invalid"; }
    else if (!input["enabled"].is<bool>()) { ok = false; error = "enabled_required"; }
    else setMaster(input["enabled"], now, "remote");
  } else if (action == "alarm.upsert") {
    const uint32_t id = input["id"] | 0;
    Schedule::Alarm* alarm = id ? findAlarm(data_, id) : freeAlarm(data_);
    const int hour = input["hour"] | 0;
    const int minute = input["minute"] | 0;
    const int weekdayMask = input["weekdayMask"] | 0;
    const int64_t epoch = input["epoch"] | static_cast<int64_t>(0);
    const bool recurring = input["recurring"] | false;
    if (!alarm) { ok = false; error = "alarm_not_found_or_full"; }
    else if (hour < 0 || hour > 23 || minute < 0 || minute > 59 ||
             (recurring && (weekdayMask <= 0 || weekdayMask > 127)) ||
             (!recurring && epoch <= now)) {
      ok = false; error = "invalid_alarm";
    } else {
      Schedule::Alarm candidate = id ? *alarm : Schedule::Alarm{};
      if (!id) candidate.id = data_.nextId++;
      candidate.used = true;
      candidate.enabled = input["enabled"] | true;
      candidate.recurring = recurring;
      candidate.temporary = false;
      candidate.hour = static_cast<uint8_t>(hour);
      candidate.minute = static_cast<uint8_t>(minute);
      candidate.weekdayMask = static_cast<uint8_t>(weekdayMask);
      candidate.oneTimeEpoch = static_cast<time_t>(epoch);
      candidate.origin = recurring ? Schedule::Origin::Recurring
                                   : Schedule::Origin::OneTime;
      candidate.state = candidate.enabled
          ? (masterEnabled_ ? Schedule::EntryState::Scheduled
                            : Schedule::EntryState::Suspended)
          : Schedule::EntryState::Disabled;
      *alarm = candidate;
      if (candidate.enabled) {
        data_.sharpAlarmGateEnabled = true;
        setSharpOutput(false);
      } else {
        clearGateWithoutEntries();
      }
      markDirty();
      needsAlarmProgramming_ = true;
    }
  } else if (action == "alarm.set_enabled") {
    Schedule::Alarm* alarm = findAlarm(data_, input["id"] | 0);
    if (!alarm || !input["enabled"].is<bool>()) {
      ok = false; error = "alarm_not_found_or_enabled_required";
    } else {
      alarm->enabled = input["enabled"];
      alarm->state = alarm->enabled
          ? (masterEnabled_ ? Schedule::EntryState::Scheduled
                            : Schedule::EntryState::Suspended)
          : Schedule::EntryState::Disabled;
      if (alarm->enabled) {
        data_.sharpAlarmGateEnabled = true;
        setSharpOutput(false);
      } else {
        clearGateWithoutEntries();
      }
      markDirty(); needsAlarmProgramming_ = true;
    }
  } else if (action == "alarm.delete") {
    Schedule::Alarm* alarm = findAlarm(data_, input["id"] | 0);
    if (!alarm) { ok = false; error = "alarm_not_found"; }
    else {
      *alarm = Schedule::Alarm{};
      clearGateWithoutEntries();
      markDirty();
      needsAlarmProgramming_ = true;
    }
  } else if (action == "timer.create") {
    const uint16_t minutes = input["minutes"] | 0;
    Schedule::Timer* timer = freeTimer(data_);
    if (!timer || minutes < 1 || minutes > Config::MAX_TIMER_MINUTES || !now) {
      ok = false; error = "invalid_timer_or_full";
    } else {
      *timer = Schedule::Timer{};
      timer->id = data_.nextId++;
      timer->used = timer->enabled = true;
      timer->targetEpoch = now + static_cast<time_t>(minutes) * 60;
      timer->state = masterEnabled_ ? Schedule::EntryState::Scheduled
                                    : Schedule::EntryState::Suspended;
      if (!masterEnabled_) timer->pausedRemainingSeconds = minutes * 60UL;
      data_.sharpAlarmGateEnabled = true;
      setSharpOutput(false);
      markDirty(); needsAlarmProgramming_ = true;
    }
  } else if (action == "timer.set_enabled") {
    Schedule::Timer* timer = findTimer(data_, input["id"] | 0);
    if (!timer || !input["enabled"].is<bool>()) {
      ok = false; error = "timer_not_found_or_enabled_required";
    } else if (input["enabled"].as<bool>() &&
               timer->targetEpoch <= now &&
               timer->pausedRemainingSeconds == 0) {
      ok = false; error = "expired_timer";
    } else {
      timer->enabled = input["enabled"];
      timer->state = timer->enabled
          ? (masterEnabled_ ? Schedule::EntryState::Scheduled
                            : Schedule::EntryState::Suspended)
          : Schedule::EntryState::Disabled;
      if (timer->enabled) {
        data_.sharpAlarmGateEnabled = true;
        setSharpOutput(false);
      } else {
        clearGateWithoutEntries();
      }
      markDirty(); needsAlarmProgramming_ = true;
    }
  } else if (action == "timer.delete") {
    Schedule::Timer* timer = findTimer(data_, input["id"] | 0);
    if (!timer) { ok = false; error = "timer_not_found"; }
    else {
      *timer = Schedule::Timer{};
      clearGateWithoutEntries();
      markDirty();
      needsAlarmProgramming_ = true;
    }
  } else if (action == "sharp_alarm.set_enabled") {
    if (!input["enabled"].is<bool>()) {
      ok = false; error = "enabled_required";
    } else if (manualFallback_) {
      ok = false; error = "manual_fallback";
    } else if (!setSharpAlarmGate(
                   input["enabled"].as<bool>(), now, "remote")) {
      ok = false; error = "no_eligible_occurrence";
    }
  } else if (action == "alarm.dismiss") {
    if (!ringing_) { ok = false; error = "not_ringing"; }
    else beginDismiss(now);
  } else if (action == "alarm.snooze") {
    ok = false; error = "unsupported_action";
  } else if (action == "clock.sync") {
    if (!(input["confirm"] | false)) { ok = false; error = "confirmation_required"; }
    else { needsClockSync_ = true; }
  } else if (action == "clock.reset") {
    if (!(input["confirm"] | false)) { ok = false; error = "confirmation_required"; }
    else if (sharp_.reset(millis()) != SharpController::Result::Accepted) {
      ok = false; error = "busy";
    }
  } else if (action == "button") {
    const ControlId id = parseControl(input["button"] | "");
    const String gesture = input["gesture"] | "click";
    if (id == ControlId::OnOff) {
      setMaster(!masterEnabled_, now, "virtual-button");
    } else if (id == ControlId::Snooze) {
      if (gesture != "click") {
        ok = false; error = "unsupported_gesture";
      } else if (!setSharpAlarmGate(
                     !data_.sharpAlarmGateEnabled, now, "virtual-button")) {
        ok = false; error = "no_eligible_occurrence";
      }
    } else {
      ok = false; error = "unsupported_button";
    }
  } else if (action == "diagnostic.button") {
    if (!(input["confirm"] | false)) { ok = false; error = "confirmation_required"; }
    else if (sharp_.tap(parseControl(input["button"] | ""), millis()) !=
             SharpController::Result::Accepted) {
      ok = false; error = "busy_or_invalid";
    }
  } else {
    ok = false; error = "unknown_action";
  }

  JsonDocument output;
  output["ok"] = ok;
  if (requestId.length()) output["requestId"] = requestId;
  if (!ok) output["error"] = error;
  serializeJson(output, response);
  if (requestId.length()) {
    recentRequestIds_[recentRequestCursor_] = requestId;
    recentResponses_[recentRequestCursor_] = response;
    recentRequestCursor_ = static_cast<uint8_t>((recentRequestCursor_ + 1) % 8);
  }
  if (ok) network_.publishState();
  return ok;
}

void SmartClockApp::printStatus() {
  Serial.println(buildStateJson());
}

void SmartClockApp::processSerial(String command) {
  command.trim();
  if (command == "status") printStatus();
  else if (command == "alarms" || command == "timers") printStatus();
  else if (command == "wifi") Serial.printf("wifi=%s ip=%s\n",
      time_.wifiConnected() ? "connected" : "offline",
      WiFi.localIP().toString().c_str());
  else if (command == "mqtt") Serial.printf("mqtt=%s\n",
      network_.mqttConnected() ? "connected" : "offline");
  else if (command == "sync") needsClockSync_ = true;
  else if (command == "test relay confirm") {
    if (sharp_.reset(millis()) != SharpController::Result::Accepted)
      Serial.println("[warn] relay test rejected: busy");
  } else if (command.startsWith("test button ")) {
    if (sharp_.tap(parseControl(command.substring(12)), millis()) !=
        SharpController::Result::Accepted)
      Serial.println("[warn] button test rejected");
  } else {
    Serial.println("status | alarms | timers | wifi | mqtt | sync");
    Serial.println("test button <name> | test relay confirm");
  }
}

void SmartClockApp::updateTerminal() {
  while (Serial.available()) {
    const char value = static_cast<char>(Serial.read());
    if (value == '\r' || value == '\n') {
      if (terminalLine_.length()) {
        processSerial(terminalLine_);
        terminalLine_ = "";
      }
    } else if (terminalLine_.length() < 256) terminalLine_ += value;
  }
}

void SmartClockApp::update() {
  const uint32_t nowMs = millis();
  time_.update(nowMs);
  if (time_.valid())
    exitManualFallback();
  else if (TimeUtils::elapsed(
               nowMs, bootMs_, Config::STARTUP_INPUT_LOCKOUT_MS))
    enterManualFallback();

  if (time_.valid() && !masterStateInitialized_) {
    masterStateInitialized_ = true;
    if (masterEnabled_) {
      Schedule::resumeTimers(data_.timers, Config::MAX_TIMERS, time_.now());
      for (auto& alarm : data_.alarms)
        if (alarm.used && alarm.enabled &&
            alarm.state == Schedule::EntryState::Suspended)
          alarm.state = Schedule::EntryState::Scheduled;
    } else {
      Schedule::suspendTimers(data_.timers, Config::MAX_TIMERS, time_.now());
      for (auto& alarm : data_.alarms)
        if (alarm.used && alarm.enabled &&
            alarm.origin != Schedule::Origin::Physical)
          alarm.state = Schedule::EntryState::Suspended;
    }
    markDirty();
  }
  if (!controlsUnlocked_ &&
      (time_.valid() ||
       TimeUtils::elapsed(nowMs, bootMs_, Config::STARTUP_INPUT_LOCKOUT_MS))) {
    controlsUnlocked_ = true;
    const bool physicalMaster = panel_.switchOn();
    setMaster(
        physicalMaster, time_.valid() ? time_.now() : 0, "startup-physical");
    if (manualFallback_) {
      fallbackSharpEnabled_ = physicalMaster;
      setSharpOutput(physicalMaster);
    }
    Serial.println("[info] physical controls unlocked");
  }

  handleInputs(nowMs);
  updateManualImport(nowMs);
  sharp_.update(nowMs, time_.nowMilliseconds());
  updateSchedule(nowMs);
  network_.update(nowMs, time_.valid());
  updateTerminal();
}
