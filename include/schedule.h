#pragma once
#include <stdint.h>
#include <time.h>

namespace Schedule {
enum class Origin : uint8_t { Pwa, Recurring, OneTime, Timer, Physical };
enum class EntryState : uint8_t {
  Disabled, Scheduled, Suspended, Ringing, Snoozed, Fired, Suppressed
};
enum class Completion : uint8_t { Fired, Suppressed };

struct Alarm {
  uint32_t id = 0;
  bool used = false;
  bool enabled = false;
  bool recurring = false;
  bool temporary = false;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t weekdayMask = 0;
  time_t oneTimeEpoch = 0;
  time_t lastFiredEpoch = 0;
  Origin origin = Origin::Pwa;
  EntryState state = EntryState::Disabled;
};

struct Timer {
  uint32_t id = 0;
  bool used = false;
  bool enabled = false;
  time_t targetEpoch = 0;
  uint32_t pausedRemainingSeconds = 0;
  Origin origin = Origin::Timer;
  EntryState state = EntryState::Disabled;
};

struct Occurrence {
  bool valid = false;
  bool timer = false;
  uint8_t index = 0;
  time_t epoch = 0;
  uint32_t id = 0;
};

time_t nextAlarmEpoch(const Alarm& alarm, time_t now);
Occurrence nextOccurrence(const Alarm* alarms, uint8_t alarmCount,
                          const Timer* timers, uint8_t timerCount,
                          bool masterEnabled, time_t now);
Occurrence dueOccurrence(const Alarm* alarms, uint8_t alarmCount,
                         const Timer* timers, uint8_t timerCount,
                         bool masterEnabled, time_t now,
                         int32_t graceSeconds);
bool isDue(time_t target, time_t now, int32_t graceSeconds);
void suspendTimers(Timer* timers, uint8_t count, time_t now);
void resumeTimers(Timer* timers, uint8_t count, time_t now);
void completeAlarm(Alarm& alarm, time_t occurrenceEpoch, Completion completion);
void completeTimer(Timer& timer, Completion completion);
bool hasEnabledEntries(const Alarm* alarms, uint8_t alarmCount,
                       const Timer* timers, uint8_t timerCount);
const char* originName(Origin origin);
const char* stateName(EntryState state);
}  // namespace Schedule
