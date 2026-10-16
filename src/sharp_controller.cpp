#include "sharp_controller.h"

#include <string.h>
#include "app_config.h"
#include "time_utils.h"

void SharpController::begin() {
  digitalWrite(Config::RELAY_PIN, HIGH);
  pinMode(Config::RELAY_PIN, OUTPUT);
  for (uint8_t pin : Config::OUTPUT_PINS) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
  }
  job_ = Job::None;
  phase_ = Phase::Idle;
}

void SharpController::write(ControlId id, bool active) {
  const uint8_t index = static_cast<uint8_t>(id);
  if (index < Config::CONTROL_COUNT) {
    digitalWrite(Config::OUTPUT_PINS[index], active ? HIGH : LOW);
  }
}

void SharpController::releaseMomentary() {
  for (uint8_t i = 0; i < 5; ++i) digitalWrite(Config::OUTPUT_PINS[i], LOW);
}

void SharpController::setManualOutput(ControlId id, bool active) {
  if (busy() || id == ControlId::Snooze || id == ControlId::OnOff) return;
  write(id, active);
}

void SharpController::setNativeAlarmEnabled(bool enabled) {
  nativeAlarmEnabled_ = enabled;
  if (!busy()) write(ControlId::OnOff, enabled);
}

void SharpController::enter(Phase phase, uint32_t nowMs) {
  phase_ = phase;
  phaseAtMs_ = nowMs;
}

SharpController::Result SharpController::tap(ControlId id, uint32_t nowMs) {
  if (busy()) return Result::Busy;
  if (id == ControlId::Count) return Result::Invalid;
  tapId_ = id;
  job_ = Job::Tap;
  lastSucceeded_ = false;
  write(id, true);
  enter(Phase::TapDown, nowMs);
  return Result::Accepted;
}

SharpController::Result SharpController::reset(uint32_t nowMs) {
  if (busy()) return Result::Busy;
  job_ = Job::Reset;
  programAlarm_ = false;
  lastSucceeded_ = false;
  enter(Phase::ReleaseOutputs, nowMs);
  return Result::Accepted;
}

SharpController::Result SharpController::synchronize(
    uint8_t hour24, uint8_t minute, int64_t targetBoundaryMs,
    bool programAlarm, uint8_t alarmHour24, uint8_t alarmMinute,
    bool alarmEnabled, uint32_t nowMs) {
  if (busy()) return Result::Busy;
  if (hour24 > 23 || minute > 59 || alarmHour24 > 23 || alarmMinute > 59) {
    return Result::Invalid;
  }
  timeHourRemaining_ = static_cast<uint8_t>(TimeUtils::hourPressesFromMidnight(hour24));
  timeMinuteRemaining_ = static_cast<uint8_t>(TimeUtils::minuteBeforeTarget(minute));
  alarmHourRemaining_ = static_cast<uint8_t>(TimeUtils::hourPressesFromMidnight(alarmHour24));
  alarmMinuteRemaining_ = alarmMinute;
  targetBoundaryMs_ = targetBoundaryMs;
  programAlarm_ = programAlarm;
  alarmEnabled_ = alarmEnabled;
  job_ = Job::Synchronize;
  lastSucceeded_ = false;
  enter(Phase::ReleaseOutputs, nowMs);
  return Result::Accepted;
}

void SharpController::fail(const char* message) {
  releaseMomentary();
  digitalWrite(Config::RELAY_PIN, HIGH);
  write(ControlId::OnOff, nativeAlarmEnabled_);
  strncpy(lastError_, message, sizeof(lastError_) - 1);
  lastError_[sizeof(lastError_) - 1] = '\0';
  lastSucceeded_ = false;
  job_ = Job::None;
  phase_ = Phase::Failed;
}

void SharpController::cancel(const char* reason) {
  if (!busy()) return;
  fail(reason);
}

void SharpController::update(uint32_t nowMs, int64_t epochMs) {
  switch (phase_) {
    case Phase::Idle:
    case Phase::Failed:
      return;
    case Phase::TapDown:
      if (TimeUtils::elapsed(nowMs, phaseAtMs_, Config::BUTTON_PRESS_MS)) {
        write(tapId_, false);
        enter(Phase::TapUp, nowMs);
      }
      return;
    case Phase::TapUp:
      if (TimeUtils::elapsed(nowMs, phaseAtMs_, Config::BUTTON_RELEASE_MS)) {
        lastSucceeded_ = true;
        lastError_[0] = '\0';
        job_ = Job::None;
        enter(Phase::Idle, nowMs);
        write(ControlId::OnOff, nativeAlarmEnabled_);
      }
      return;
    case Phase::ReleaseOutputs:
      releaseMomentary();
      write(ControlId::OnOff, false);
      digitalWrite(Config::RELAY_PIN, LOW);
      enter(Phase::PowerOff, nowMs);
      return;
    case Phase::PowerOff:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::RELAY_OFF_MS)) return;
      digitalWrite(Config::RELAY_PIN, HIGH);
      enter(Phase::Boot, nowMs);
      return;
    case Phase::Boot:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::SHARP_BOOT_MS)) return;
      if (job_ == Job::Reset) {
        enter(Phase::Finish, nowMs);
      } else {
        write(ControlId::Time, true);
        enter(Phase::HoldTime, nowMs);
      }
      return;
    case Phase::HoldTime:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::MODE_START_MS)) return;
      if (timeHourRemaining_ > 0) {
        write(ControlId::Hour, true);
        enter(Phase::TimeHourDown, nowMs);
      } else if (timeMinuteRemaining_ > 0) {
        write(ControlId::Minute, true);
        enter(Phase::TimeMinuteDown, nowMs);
      } else {
        enter(Phase::WaitBoundary, nowMs);
      }
      return;
    case Phase::TimeHourDown:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::BUTTON_PRESS_MS)) return;
      write(ControlId::Hour, false);
      --timeHourRemaining_;
      enter(Phase::TimeHourUp, nowMs);
      return;
    case Phase::TimeHourUp:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::BUTTON_RELEASE_MS)) return;
      if (timeHourRemaining_ > 0) {
        write(ControlId::Hour, true);
        enter(Phase::TimeHourDown, nowMs);
      } else if (timeMinuteRemaining_ > 0) {
        write(ControlId::Minute, true);
        enter(Phase::TimeMinuteDown, nowMs);
      } else {
        enter(Phase::WaitBoundary, nowMs);
      }
      return;
    case Phase::TimeMinuteDown:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::BUTTON_PRESS_MS)) return;
      write(ControlId::Minute, false);
      --timeMinuteRemaining_;
      enter(Phase::TimeMinuteUp, nowMs);
      return;
    case Phase::TimeMinuteUp:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::BUTTON_RELEASE_MS)) return;
      if (timeMinuteRemaining_ > 0) {
        write(ControlId::Minute, true);
        enter(Phase::TimeMinuteDown, nowMs);
      } else {
        enter(Phase::WaitBoundary, nowMs);
      }
      return;
    case Phase::WaitBoundary:
      if (epochMs <= 0) {
        fail("time became invalid during synchronization");
      } else if (epochMs >= targetBoundaryMs_) {
        if (epochMs - targetBoundaryMs_ > 1000) {
          fail("missed synchronization boundary");
        } else {
          write(ControlId::Minute, true);
          enter(Phase::FinalMinuteDown, nowMs);
        }
      }
      return;
    case Phase::FinalMinuteDown:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::BUTTON_PRESS_MS)) return;
      write(ControlId::Minute, false);
      enter(Phase::FinalMinuteUp, nowMs);
      return;
    case Phase::FinalMinuteUp:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::BUTTON_RELEASE_MS)) return;
      enter(Phase::ReleaseTime, nowMs);
      return;
    case Phase::ReleaseTime:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::MODE_END_MS)) return;
      write(ControlId::Time, false);
      if (!programAlarm_) {
        enter(Phase::Finish, nowMs);
      } else {
        write(ControlId::Alarm, true);
        enter(Phase::HoldAlarm, nowMs);
      }
      return;
    case Phase::HoldAlarm:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::MODE_START_MS)) return;
      if (alarmHourRemaining_ > 0) {
        write(ControlId::Hour, true);
        enter(Phase::AlarmHourDown, nowMs);
      } else if (alarmMinuteRemaining_ > 0) {
        write(ControlId::Minute, true);
        enter(Phase::AlarmMinuteDown, nowMs);
      } else {
        enter(Phase::ReleaseAlarm, nowMs);
      }
      return;
    case Phase::AlarmHourDown:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::BUTTON_PRESS_MS)) return;
      write(ControlId::Hour, false);
      --alarmHourRemaining_;
      enter(Phase::AlarmHourUp, nowMs);
      return;
    case Phase::AlarmHourUp:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::BUTTON_RELEASE_MS)) return;
      if (alarmHourRemaining_ > 0) {
        write(ControlId::Hour, true);
        enter(Phase::AlarmHourDown, nowMs);
      } else if (alarmMinuteRemaining_ > 0) {
        write(ControlId::Minute, true);
        enter(Phase::AlarmMinuteDown, nowMs);
      } else {
        enter(Phase::ReleaseAlarm, nowMs);
      }
      return;
    case Phase::AlarmMinuteDown:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::BUTTON_PRESS_MS)) return;
      write(ControlId::Minute, false);
      --alarmMinuteRemaining_;
      enter(Phase::AlarmMinuteUp, nowMs);
      return;
    case Phase::AlarmMinuteUp:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::BUTTON_RELEASE_MS)) return;
      if (alarmMinuteRemaining_ > 0) {
        write(ControlId::Minute, true);
        enter(Phase::AlarmMinuteDown, nowMs);
      } else {
        enter(Phase::ReleaseAlarm, nowMs);
      }
      return;
    case Phase::ReleaseAlarm:
      if (!TimeUtils::elapsed(nowMs, phaseAtMs_, Config::MODE_END_MS)) return;
      write(ControlId::Alarm, false);
      enter(Phase::Finish, nowMs);
      return;
    case Phase::Finish:
      nativeAlarmEnabled_ = programAlarm_ ? alarmEnabled_ : nativeAlarmEnabled_;
      write(ControlId::OnOff, nativeAlarmEnabled_);
      lastSucceeded_ = true;
      lastError_[0] = '\0';
      job_ = Job::None;
      enter(Phase::Idle, nowMs);
      return;
  }
}
