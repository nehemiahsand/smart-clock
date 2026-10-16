#include <unity.h>
#include <stdlib.h>
#include <time.h>
#include "schedule.h"

using namespace Schedule;

time_t localEpoch(int year, int month, int day, int hour, int minute) {
  tm value{};
  value.tm_year = year - 1900;
  value.tm_mon = month - 1;
  value.tm_mday = day;
  value.tm_hour = hour;
  value.tm_min = minute;
  value.tm_isdst = -1;
  return mktime(&value);
}

void setUp() {
  setenv("TZ", "CST6CDT,M3.2.0,M11.1.0", 1);
  tzset();
}
void tearDown() {}

void test_one_time_and_timer_selection() {
  const time_t now = localEpoch(2026, 1, 15, 10, 0);
  Alarm alarms[1]{};
  alarms[0].used = alarms[0].enabled = true;
  alarms[0].id = 10;
  alarms[0].oneTimeEpoch = now + 600;
  Timer timers[1]{};
  timers[0].used = timers[0].enabled = true;
  timers[0].id = 20;
  timers[0].targetEpoch = now + 300;
  const Occurrence next = nextOccurrence(alarms, 1, timers, 1, true, now);
  TEST_ASSERT_TRUE(next.valid);
  TEST_ASSERT_TRUE(next.timer);
  TEST_ASSERT_EQUAL_UINT32(20, next.id);
}

void test_master_off_allows_only_physical_alarm() {
  const time_t now = localEpoch(2026, 1, 15, 10, 0);
  Alarm alarms[2]{};
  for (auto& alarm : alarms) {
    alarm.used = alarm.enabled = true;
    alarm.oneTimeEpoch = now + 60;
  }
  alarms[0].id = 1;
  alarms[0].origin = Origin::Pwa;
  alarms[1].id = 2;
  alarms[1].origin = Origin::Physical;
  const Occurrence next = nextOccurrence(alarms, 2, nullptr, 0, false, now);
  TEST_ASSERT_TRUE(next.valid);
  TEST_ASSERT_EQUAL_UINT32(2, next.id);
}

void test_timer_pause_and_resume() {
  Timer timer{};
  timer.used = timer.enabled = true;
  timer.targetEpoch = 1100;
  suspendTimers(&timer, 1, 1000);
  TEST_ASSERT_EQUAL_UINT32(100, timer.pausedRemainingSeconds);
  TEST_ASSERT_EQUAL(EntryState::Suspended, timer.state);
  resumeTimers(&timer, 1, 2000);
  TEST_ASSERT_EQUAL_INT64(2100, timer.targetEpoch);
  TEST_ASSERT_EQUAL(EntryState::Scheduled, timer.state);
}

void test_due_occurrence_honors_five_minute_grace() {
  const time_t now = localEpoch(2026, 1, 15, 10, 5);
  Alarm alarms[1]{};
  alarms[0].used = alarms[0].enabled = true;
  alarms[0].id = 41;
  alarms[0].oneTimeEpoch = now - 299;
  auto due = dueOccurrence(alarms, 1, nullptr, 0, true, now, 300);
  TEST_ASSERT_TRUE(due.valid);
  TEST_ASSERT_EQUAL_UINT32(41, due.id);
  alarms[0].oneTimeEpoch = now - 301;
  due = dueOccurrence(alarms, 1, nullptr, 0, true, now, 300);
  TEST_ASSERT_FALSE(due.valid);
}

void test_spring_nonexistent_time_is_skipped() {
  const time_t now = localEpoch(2026, 3, 7, 23, 0);
  Alarm alarm{};
  alarm.used = alarm.enabled = alarm.recurring = true;
  alarm.hour = 2;
  alarm.minute = 30;
  alarm.weekdayMask = 1U << 0;
  TEST_ASSERT_EQUAL_INT64(0, nextAlarmEpoch(alarm, now));
}

void test_fall_alarm_has_one_selected_epoch() {
  const time_t now = localEpoch(2026, 10, 31, 23, 0);
  Alarm alarm{};
  alarm.used = alarm.enabled = alarm.recurring = true;
  alarm.hour = 1;
  alarm.minute = 30;
  alarm.weekdayMask = 1U << 0;
  const time_t first = nextAlarmEpoch(alarm, now);
  TEST_ASSERT_GREATER_THAN_INT64(now, first);
  alarm.lastFiredEpoch = first;
  TEST_ASSERT_EQUAL_INT64(0, nextAlarmEpoch(alarm, now));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_one_time_and_timer_selection);
  RUN_TEST(test_master_off_allows_only_physical_alarm);
  RUN_TEST(test_timer_pause_and_resume);
  RUN_TEST(test_due_occurrence_honors_five_minute_grace);
  RUN_TEST(test_spring_nonexistent_time_is_skipped);
  RUN_TEST(test_fall_alarm_has_one_selected_epoch);
  return UNITY_END();
}
