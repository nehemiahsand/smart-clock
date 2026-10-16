#pragma once
#include <stdint.h>

struct DebounceResult {
  bool changed = false;
  bool pressed = false;
  bool clicked = false;
  bool longPressed = false;
};

class DebouncedInput {
 public:
  DebouncedInput(uint32_t debounceMs = 35, uint32_t longPressMs = 2000);
  void initialize(bool pressed, uint32_t now);
  DebounceResult update(bool rawPressed, uint32_t now);
  bool isPressed() const { return stablePressed_; }

 private:
  uint32_t debounceMs_;
  uint32_t longPressMs_;
  bool initialized_ = false;
  bool rawPressed_ = false;
  bool stablePressed_ = false;
  bool longEmitted_ = false;
  uint32_t rawChangedAt_ = 0;
  uint32_t pressedAt_ = 0;
};
