#pragma once
#include <Arduino.h>
#include <time.h>

class TimeService {
 public:
  void begin(uint32_t nowMs);
  void update(uint32_t nowMs);
  bool valid() const { return valid_; }
  bool wifiConnected() const;
  time_t now() const;
  int64_t nowMilliseconds() const;
  bool local(tm& value) const;
  bool consumeSignificantStep();
  const char* status() const;

 private:
  bool valid_ = false;
  bool ntpConfigured_ = false;
  bool significantStep_ = false;
  uint32_t lastWifiAttemptMs_ = 0;
  uint32_t lastSampleMs_ = 0;
  time_t lastSampleEpoch_ = 0;
};
