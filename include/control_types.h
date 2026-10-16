#pragma once
#include <stdint.h>

enum class ControlId : uint8_t {
  Time = 0,
  Hour,
  Minute,
  Alarm,
  Snooze,
  OnOff,
  Count
};

enum class InputEventType : uint8_t {
  Pressed,
  Released,
  Clicked,
  LongPressed,
  SwitchOn,
  SwitchOff
};

struct InputEvent {
  ControlId control;
  InputEventType type;
  uint32_t atMs;
};

inline const char* controlName(ControlId id) {
  switch (id) {
    case ControlId::Time: return "time";
    case ControlId::Hour: return "hour";
    case ControlId::Minute: return "min";
    case ControlId::Alarm: return "alarm";
    case ControlId::Snooze: return "snooze";
    case ControlId::OnOff: return "onoff";
    default: return "unknown";
  }
}
