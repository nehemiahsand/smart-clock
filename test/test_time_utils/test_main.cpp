#include <unity.h>
#include "time_utils.h"

void test_elapsed_is_wrap_safe() {
  const uint32_t start = 0xfffffff0UL;
  TEST_ASSERT_FALSE(TimeUtils::elapsed(0xfffffff5UL, start, 20));
  TEST_ASSERT_TRUE(TimeUtils::elapsed(0x00000004UL, start, 20));
}
void test_reached_is_wrap_safe() {
  TEST_ASSERT_FALSE(TimeUtils::reached(90, 100));
  TEST_ASSERT_TRUE(TimeUtils::reached(100, 100));
  TEST_ASSERT_TRUE(TimeUtils::reached(110, 100));
}
void test_press_calculations() {
  TEST_ASSERT_EQUAL(0, TimeUtils::hourPressesFromMidnight(0));
  TEST_ASSERT_EQUAL(23, TimeUtils::hourPressesFromMidnight(23));
  TEST_ASSERT_EQUAL(0, TimeUtils::hourPressesFromMidnight(24));
  TEST_ASSERT_EQUAL(59, TimeUtils::minuteBeforeTarget(0));
  TEST_ASSERT_EQUAL(0, TimeUtils::minuteBeforeTarget(1));
}
void test_forward_alarm_press_calculations() {
  TEST_ASSERT_EQUAL(0, TimeUtils::forwardHourPresses(6, 6));
  TEST_ASSERT_EQUAL(2, TimeUtils::forwardHourPresses(23, 1));
  TEST_ASSERT_EQUAL(23, TimeUtils::forwardHourPresses(1, 0));
  TEST_ASSERT_EQUAL(0, TimeUtils::forwardMinutePresses(30, 30));
  TEST_ASSERT_EQUAL(10, TimeUtils::forwardMinutePresses(55, 5));
  TEST_ASSERT_EQUAL(59, TimeUtils::forwardMinutePresses(1, 0));
}
void test_sequence_estimate() {
  TEST_ASSERT_EQUAL_UINT32(
      2100, TimeUtils::estimateButtonSequenceMs(1, 2, 600, 250, 150, 300));
}
int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_elapsed_is_wrap_safe);
  RUN_TEST(test_reached_is_wrap_safe);
  RUN_TEST(test_press_calculations);
  RUN_TEST(test_forward_alarm_press_calculations);
  RUN_TEST(test_sequence_estimate);
  return UNITY_END();
}
