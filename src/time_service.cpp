#include "time_service.h"

#include <WiFi.h>
#include <sys/time.h>
#include "app_config.h"
#include "time_utils.h"

void TimeService::begin(uint32_t nowMs) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(Config::HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  lastWifiAttemptMs_ = nowMs;
}

bool TimeService::wifiConnected() const {
  return WiFi.status() == WL_CONNECTED;
}

time_t TimeService::now() const {
  const time_t value = time(nullptr);
  return value >= 1700000000 ? value : 0;
}

int64_t TimeService::nowMilliseconds() const {
  timeval value{};
  gettimeofday(&value, nullptr);
  if (value.tv_sec < 1700000000) return 0;
  return static_cast<int64_t>(value.tv_sec) * 1000 + value.tv_usec / 1000;
}

bool TimeService::local(tm& value) const {
  const time_t epoch = now();
  if (!epoch) return false;
  localtime_r(&epoch, &value);
  return true;
}

void TimeService::update(uint32_t nowMs) {
  if (!wifiConnected()) {
    ntpConfigured_ = false;
    if (TimeUtils::elapsed(nowMs, lastWifiAttemptMs_, Config::WIFI_RETRY_MS)) {
      lastWifiAttemptMs_ = nowMs;
      WiFi.disconnect();
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  } else if (!ntpConfigured_) {
    configTzTime(Config::TZ_RULE, Config::NTP_PRIMARY, Config::NTP_SECONDARY);
    ntpConfigured_ = true;
  }

  const time_t epoch = now();
  if (!epoch) return;
  if (valid_ && lastSampleEpoch_ != 0) {
    const time_t expected =
        lastSampleEpoch_ + static_cast<time_t>((nowMs - lastSampleMs_) / 1000);
    const time_t difference = epoch > expected ? epoch - expected : expected - epoch;
    if (difference >= Config::SIGNIFICANT_TIME_STEP_SECONDS)
      significantStep_ = true;
  }
  valid_ = true;
  lastSampleEpoch_ = epoch;
  lastSampleMs_ = nowMs;
}

bool TimeService::consumeSignificantStep() {
  const bool result = significantStep_;
  significantStep_ = false;
  return result;
}

const char* TimeService::status() const {
  if (!wifiConnected()) return valid_ ? "offline-clock-valid" : "offline-time-invalid";
  return valid_ ? "online-time-valid" : "online-waiting-ntp";
}
