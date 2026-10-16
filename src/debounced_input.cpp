#include "debounced_input.h"
#include "time_utils.h"

DebouncedInput::DebouncedInput(uint32_t debounceMs, uint32_t longPressMs)
    : debounceMs_(debounceMs), longPressMs_(longPressMs) {}

void DebouncedInput::initialize(bool pressed, uint32_t now) {
  initialized_ = true;
  rawPressed_ = pressed;
  stablePressed_ = pressed;
  longEmitted_ = false;
  rawChangedAt_ = now;
  pressedAt_ = now;
}

DebounceResult DebouncedInput::update(bool rawPressed, uint32_t now) {
  DebounceResult result;
  if (!initialized_) {
    initialize(rawPressed, now);
    return result;
  }

  if (rawPressed != rawPressed_) {
    rawPressed_ = rawPressed;
    rawChangedAt_ = now;
  }

  if (rawPressed_ != stablePressed_ &&
      TimeUtils::elapsed(now, rawChangedAt_, debounceMs_)) {
    stablePressed_ = rawPressed_;
    result.changed = true;
    result.pressed = stablePressed_;
    if (stablePressed_) {
      pressedAt_ = now;
      longEmitted_ = false;
    } else {
      result.clicked = !longEmitted_;
    }
  }

  if (stablePressed_ && !longEmitted_ &&
      TimeUtils::elapsed(now, pressedAt_, longPressMs_)) {
    longEmitted_ = true;
    result.longPressed = true;
    result.pressed = true;
  }
  return result;
}
