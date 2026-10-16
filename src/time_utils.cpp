#include "time_utils.h"

namespace TimeUtils {
int positiveModulo(int value, int modulus) {
  const int remainder = value % modulus;
  return remainder < 0 ? remainder + modulus : remainder;
}
int hourPressesFromMidnight(int hour24) {
  return positiveModulo(hour24, 24);
}
int minuteBeforeTarget(int minute) {
  return positiveModulo(minute - 1, 60);
}
uint32_t estimateButtonSequenceMs(int hourPresses, int minutePresses,
                                  uint32_t modeStartMs, uint32_t pressMs,
                                  uint32_t releaseMs, uint32_t modeEndMs) {
  const uint32_t pulses = static_cast<uint32_t>(hourPresses + minutePresses);
  return modeStartMs + pulses * (pressMs + releaseMs) + modeEndMs;
}
}  // namespace TimeUtils
