#pragma once
#include <Preferences.h>
#include "app_config.h"
#include "schedule.h"

struct PersistedData {
  uint32_t nextId = 1;
  Schedule::Alarm alarms[Config::MAX_ALARMS]{};
  Schedule::Timer timers[Config::MAX_TIMERS]{};
};

class PersistentStore {
 public:
  bool begin();
  bool load(PersistedData& data);
  bool save(const PersistedData& data);
  const char* lastError() const { return lastError_; }

 private:
  Preferences preferences_;
  char lastError_[64] = "";
};
