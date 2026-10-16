#pragma once
#include <Arduino.h>
#include "control_types.h"
#include "debounced_input.h"

class ControlPanel {
 public:
  void begin(uint32_t now);
  void update(uint32_t now);
  bool pop(InputEvent& event);
  bool isPressed(ControlId id) const;
  bool switchOn() const { return inputs_[5].isPressed(); }
  uint32_t droppedEvents() const { return droppedEvents_; }

 private:
  static constexpr uint8_t QUEUE_SIZE = 16;
  DebouncedInput inputs_[6];
  InputEvent queue_[QUEUE_SIZE]{};
  uint8_t head_ = 0;
  uint8_t tail_ = 0;
  uint32_t droppedEvents_ = 0;
  void push(ControlId id, InputEventType type, uint32_t now);
};
