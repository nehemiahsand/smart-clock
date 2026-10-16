#include "schedule.h"

namespace Schedule {
namespace {
bool sameRequestedLocalTime(time_t epoch, int year, int month, int day,
                            int hour, int minute) {
  tm actual{};
  localtime_r(&epoch, &actual);
  return actual.tm_year == year && actual.tm_mon == month &&
         actual.tm_mday == day && actual.tm_hour == hour &&
         actual.tm_min == minute;
}
}

time_t nextAlarmEpoch(const Alarm& alarm, time_t now) {
  if (!alarm.used || !alarm.enabled) return 0;
  if (!alarm.recurring) return alarm.oneTimeEpoch > now ? alarm.oneTimeEpoch : 0;

  tm base{};
  localtime_r(&now, &base);
  base.tm_sec = 0;
  for (int offset = 0; offset <= 7; ++offset) {
    tm candidate = base;
    candidate.tm_mday += offset;
    candidate.tm_hour = alarm.hour;
    candidate.tm_min = alarm.minute;
    candidate.tm_sec = 0;
    candidate.tm_isdst = -1;
    const time_t epoch = mktime(&candidate);
    if (epoch <= now) continue;

    tm normalizedDay = base;
    normalizedDay.tm_mday += offset;
    normalizedDay.tm_hour = 12;
    normalizedDay.tm_min = 0;
    normalizedDay.tm_sec = 0;
    normalizedDay.tm_isdst = -1;
    mktime(&normalizedDay);
    if ((alarm.weekdayMask & (1U << normalizedDay.tm_wday)) == 0) continue;
    if (!sameRequestedLocalTime(epoch, normalizedDay.tm_year,
                                normalizedDay.tm_mon, normalizedDay.tm_mday,
                                alarm.hour, alarm.minute)) {
      continue;
    }
    if (alarm.lastFiredEpoch != 0 && epoch == alarm.lastFiredEpoch) continue;
    return epoch;
  }
  return 0;
}

Occurrence nextOccurrence(const Alarm* alarms, uint8_t alarmCount,
                          const Timer* timers, uint8_t timerCount,
                          bool masterEnabled, time_t now) {
  Occurrence best;
  for (uint8_t i = 0; i < alarmCount; ++i) {
    const Alarm& alarm = alarms[i];
    const bool allowed = masterEnabled || alarm.origin == Origin::Physical;
    if (!allowed) continue;
    const time_t epoch = nextAlarmEpoch(alarm, now);
    if (epoch > 0 && (!best.valid || epoch < best.epoch)) {
      best.valid = true;
      best.timer = false;
      best.index = i;
      best.epoch = epoch;
      best.id = alarm.id;
    }
  }
  if (masterEnabled) {
    for (uint8_t i = 0; i < timerCount; ++i) {
      const Timer& timer = timers[i];
      if (!timer.used || !timer.enabled || timer.targetEpoch <= now) continue;
      if (!best.valid || timer.targetEpoch < best.epoch) {
        best.valid = true;
        best.timer = true;
        best.index = i;
        best.epoch = timer.targetEpoch;
        best.id = timer.id;
      }
    }
  }
  return best;
}

Occurrence dueOccurrence(const Alarm* alarms, uint8_t alarmCount,
                         const Timer* timers, uint8_t timerCount,
                         bool masterEnabled, time_t now,
                         int32_t graceSeconds) {
  Occurrence best;
  tm current{};
  localtime_r(&now, &current);

  for (uint8_t i = 0; i < alarmCount; ++i) {
    const Alarm& alarm = alarms[i];
    if (!alarm.used || !alarm.enabled ||
        (!masterEnabled && alarm.origin != Origin::Physical)) continue;

    time_t target = alarm.oneTimeEpoch;
    if (alarm.recurring) {
      if ((alarm.weekdayMask & (1U << current.tm_wday)) == 0) continue;
      tm candidate = current;
      candidate.tm_hour = alarm.hour;
      candidate.tm_min = alarm.minute;
      candidate.tm_sec = 0;
      candidate.tm_isdst = -1;
      target = mktime(&candidate);
      if (!sameRequestedLocalTime(target, current.tm_year, current.tm_mon,
                                  current.tm_mday, alarm.hour, alarm.minute))
        continue;
    }
    if (!isDue(target, now, graceSeconds) || alarm.lastFiredEpoch == target)
      continue;
    if (!best.valid || target < best.epoch) {
      best.valid = true;
      best.timer = false;
      best.index = i;
      best.epoch = target;
      best.id = alarm.id;
    }
  }

  if (masterEnabled) {
    for (uint8_t i = 0; i < timerCount; ++i) {
      const Timer& timer = timers[i];
      if (!timer.used || !timer.enabled ||
          !isDue(timer.targetEpoch, now, graceSeconds)) continue;
      if (!best.valid || timer.targetEpoch < best.epoch) {
        best.valid = true;
        best.timer = true;
        best.index = i;
        best.epoch = timer.targetEpoch;
        best.id = timer.id;
      }
    }
  }
  return best;
}

bool isDue(time_t target, time_t now, int32_t graceSeconds) {
  return target > 0 && now >= target && now - target <= graceSeconds;
}

void suspendTimers(Timer* timers, uint8_t count, time_t now) {
  for (uint8_t i = 0; i < count; ++i) {
    Timer& timer = timers[i];
    if (!timer.used || !timer.enabled) continue;
    timer.pausedRemainingSeconds =
        timer.targetEpoch > now ? static_cast<uint32_t>(timer.targetEpoch - now) : 0;
    timer.state = EntryState::Suspended;
  }
}

void resumeTimers(Timer* timers, uint8_t count, time_t now) {
  for (uint8_t i = 0; i < count; ++i) {
    Timer& timer = timers[i];
    if (!timer.used || !timer.enabled ||
        timer.state != EntryState::Suspended) continue;
    if (timer.pausedRemainingSeconds == 0) {
      timer.enabled = false;
      timer.state = EntryState::Disabled;
    } else {
      timer.targetEpoch = now + timer.pausedRemainingSeconds;
      timer.pausedRemainingSeconds = 0;
      timer.state = EntryState::Scheduled;
    }
  }
}

const char* originName(Origin origin) {
  switch (origin) {
    case Origin::Pwa: return "pwa";
    case Origin::Recurring: return "recurring";
    case Origin::OneTime: return "one-time";
    case Origin::Timer: return "timer";
    case Origin::Physical: return "physical";
  }
  return "unknown";
}

const char* stateName(EntryState state) {
  switch (state) {
    case EntryState::Disabled: return "disabled";
    case EntryState::Scheduled: return "scheduled";
    case EntryState::Suspended: return "suspended";
    case EntryState::Ringing: return "ringing";
    case EntryState::Snoozed: return "snoozed";
    case EntryState::Fired: return "fired";
  }
  return "unknown";
}
}  // namespace Schedule
