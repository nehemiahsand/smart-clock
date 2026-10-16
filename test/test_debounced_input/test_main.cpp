#include <unity.h>
#include "debounced_input.h"

void test_rejects_bounce_and_clicks_on_release() {
  DebouncedInput input(35, 2000);
  input.initialize(false, 0);
  TEST_ASSERT_FALSE(input.update(true, 10).changed);
  TEST_ASSERT_FALSE(input.update(false, 20).changed);
  TEST_ASSERT_FALSE(input.update(true, 30).changed);
  auto pressed = input.update(true, 65);
  TEST_ASSERT_TRUE(pressed.changed);
  TEST_ASSERT_TRUE(pressed.pressed);
  input.update(false, 100);
  auto released = input.update(false, 135);
  TEST_ASSERT_TRUE(released.changed);
  TEST_ASSERT_TRUE(released.clicked);
}
void test_long_press_emits_once_and_suppresses_click() {
  DebouncedInput input(35, 2000);
  input.initialize(false, 0);
  input.update(true, 1);
  input.update(true, 36);
  TEST_ASSERT_FALSE(input.update(true, 2035).longPressed);
  TEST_ASSERT_TRUE(input.update(true, 2036).longPressed);
  TEST_ASSERT_FALSE(input.update(true, 3000).longPressed);
  input.update(false, 3001);
  auto released = input.update(false, 3036);
  TEST_ASSERT_FALSE(released.clicked);
}
void test_timing_survives_wrap() {
  DebouncedInput input(20, 50);
  input.initialize(false, 0xfffffff0UL);
  input.update(true, 0xfffffff5UL);
  auto pressed = input.update(true, 9);
  TEST_ASSERT_TRUE(pressed.changed);
  auto held = input.update(true, 59);
  TEST_ASSERT_TRUE(held.longPressed);
}
int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_rejects_bounce_and_clicks_on_release);
  RUN_TEST(test_long_press_emits_once_and_suppresses_click);
  RUN_TEST(test_timing_survives_wrap);
  return UNITY_END();
}
