#pragma once
#include <Arduino.h>
#include <time.h>
#include "control_types.h"

class SharpController {
 public:
  enum class Result : uint8_t { Accepted, Busy, Invalid };
  enum class Job : uint8_t { None, Tap, Reset, Synchronize };
  enum class Phase : uint8_t {
    Idle, TapDown, TapUp, ReleaseOutputs, PowerOff, Boot,
    HoldTime, TimeHourDown, TimeHourUp, TimeMinuteDown, TimeMinuteUp,
    WaitBoundary, FinalMinuteDown, FinalMinuteUp, ReleaseTime,
    HoldAlarm, AlarmHourDown, AlarmHourUp, AlarmMinuteDown, AlarmMinuteUp,
    ReleaseAlarm, Finish, Failed
  };

  void begin();
  void update(uint32_t nowMs, int64_t epochMs);
  Result tap(ControlId id, uint32_t nowMs);
  Result reset(uint32_t nowMs);
  Result synchronize(uint8_t hour24, uint8_t minute, int64_t targetBoundaryMs,
                     bool programAlarm, uint8_t alarmHour24,
                     uint8_t alarmMinute, bool alarmEnabled, uint32_t nowMs);
  void setManualOutput(ControlId id, bool active);
  void setNativeAlarmEnabled(bool enabled);
  void cancel(const char* reason);
  bool busy() const { return job_ != Job::None; }
  Job job() const { return job_; }
  Phase phase() const { return phase_; }
  bool lastSucceeded() const { return lastSucceeded_; }
  const char* lastError() const { return lastError_; }

 private:
  Job job_ = Job::None;
  Phase phase_ = Phase::Idle;
  ControlId tapId_ = ControlId::Time;
  uint32_t phaseAtMs_ = 0;
  uint8_t timeHourRemaining_ = 0;
  uint8_t timeMinuteRemaining_ = 0;
  uint8_t alarmHourRemaining_ = 0;
  uint8_t alarmMinuteRemaining_ = 0;
  int64_t targetBoundaryMs_ = 0;
  bool programAlarm_ = false;
  bool alarmEnabled_ = false;
  bool nativeAlarmEnabled_ = false;
  bool lastSucceeded_ = true;
  char lastError_[80] = "";

  void enter(Phase phase, uint32_t nowMs);
  void write(ControlId id, bool active);
  void releaseMomentary();
  void fail(const char* message);
};
