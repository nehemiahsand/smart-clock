#include "control_panel.h"
#include "app_config.h"

void ControlPanel::begin(uint32_t now) {
  for (uint8_t i = 0; i < Config::CONTROL_COUNT; ++i) {
    pinMode(Config::INPUT_PINS[i], INPUT_PULLUP);
    inputs_[i].initialize(digitalRead(Config::INPUT_PINS[i]) == LOW, now);
  }
}

void ControlPanel::push(ControlId id, InputEventType type, uint32_t now) {
  const uint8_t next = static_cast<uint8_t>((head_ + 1) % QUEUE_SIZE);
  if (next == tail_) {
    ++droppedEvents_;
    return;
  }
  queue_[head_] = {id, type, now};
  head_ = next;
}

void ControlPanel::update(uint32_t now) {
  for (uint8_t i = 0; i < Config::CONTROL_COUNT; ++i) {
    const bool rawPressed = digitalRead(Config::INPUT_PINS[i]) == LOW;
    const DebounceResult result = inputs_[i].update(rawPressed, now);
    const ControlId id = static_cast<ControlId>(i);

    if (result.changed) {
      push(id, result.pressed ? InputEventType::Pressed
                              : InputEventType::Released, now);
      if (id == ControlId::OnOff) {
        push(id, result.pressed ? InputEventType::SwitchOn
                                : InputEventType::SwitchOff, now);
      }
    }
    if (result.clicked && id != ControlId::OnOff) {
      push(id, InputEventType::Clicked, now);
    }
    if (result.longPressed && id != ControlId::OnOff) {
      push(id, InputEventType::LongPressed, now);
    }
  }
}

bool ControlPanel::pop(InputEvent& event) {
  if (tail_ == head_) return false;
  event = queue_[tail_];
  tail_ = static_cast<uint8_t>((tail_ + 1) % QUEUE_SIZE);
  return true;
}

bool ControlPanel::isPressed(ControlId id) const {
  return inputs_[static_cast<uint8_t>(id)].isPressed();
}
