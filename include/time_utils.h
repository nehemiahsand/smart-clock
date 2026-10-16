#pragma once
#include <stdint.h>

namespace TimeUtils {
inline bool elapsed(uint32_t now, uint32_t since, uint32_t interval) {
  return static_cast<uint32_t>(now - since) >= interval;
}
inline bool reached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}
int positiveModulo(int value, int modulus);
int hourPressesFromMidnight(int hour24);
int minuteBeforeTarget(int minute);
uint32_t estimateButtonSequenceMs(int hourPresses, int minutePresses,
                                  uint32_t modeStartMs, uint32_t pressMs,
                                  uint32_t releaseMs, uint32_t modeEndMs);
}  // namespace TimeUtils
