#pragma once

#include <stdint.h>
#include "secrets.h"

#ifndef MQTT_HOST
#define MQTT_HOST "nehemiah-pi5"
#endif
#ifndef MQTT_PORT
#define MQTT_PORT 8883
#endif
#ifndef MQTT_USERNAME
#define MQTT_USERNAME ""
#endif
#ifndef MQTT_PASSWORD
#define MQTT_PASSWORD ""
#endif
#ifndef MQTT_CA_CERT
#define MQTT_CA_CERT ""
#endif
#ifndef HTTP_BEARER_TOKEN
#define HTTP_BEARER_TOKEN ""
#endif
#ifndef OTA_PASSWORD
#define OTA_PASSWORD ""
#endif

namespace Config {
constexpr char DEVICE_ID[] = "smart-clock";
constexpr char HOSTNAME[] = "smart-clock";
constexpr char TZ_RULE[] = "CST6CDT,M3.2.0,M11.1.0";
constexpr char NTP_PRIMARY[] = "pool.ntp.org";
constexpr char NTP_SECONDARY[] = "time.nist.gov";
constexpr uint8_t RELAY_PIN = 23;
constexpr uint8_t INPUT_PINS[] = {18, 19, 21, 22, 16, 17};
constexpr uint8_t OUTPUT_PINS[] = {25, 26, 27, 14, 13, 33};
constexpr uint8_t CONTROL_COUNT = 6;
constexpr uint32_t DEBOUNCE_MS = 35;
constexpr uint32_t LONG_PRESS_MS = 2000;
constexpr uint32_t BUTTON_PRESS_MS = 250;
constexpr uint32_t BUTTON_RELEASE_MS = 150;
constexpr uint32_t MODE_START_MS = 600;
constexpr uint32_t MODE_END_MS = 300;
constexpr uint32_t RELAY_OFF_MS = 10000;
constexpr uint32_t SHARP_BOOT_MS = 3000;
constexpr uint32_t SHARP_JOB_TIMEOUT_MS = 180000;
constexpr uint32_t STARTUP_INPUT_LOCKOUT_MS = 30000;
constexpr uint32_t MANUAL_ALARM_SETTLE_MS = 60000;
constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t MQTT_RETRY_MS = 5000;
constexpr uint32_t STATE_PUBLISH_MS = 30000;
constexpr uint32_t RING_WINDOW_MS = 5UL * 60UL * 1000UL;
constexpr uint32_t SNOOZE_MS = 5UL * 60UL * 1000UL;
constexpr int32_t MISSED_GRACE_SECONDS = 5 * 60;
constexpr int32_t SIGNIFICANT_TIME_STEP_SECONDS = 30;
constexpr uint8_t DAILY_SYNC_HOUR = 12;
constexpr uint8_t DAILY_SYNC_MINUTE = 0;
constexpr uint8_t MAX_ALARMS = 10;
constexpr uint8_t MAX_TIMERS = 5;
constexpr uint16_t MAX_TIMER_MINUTES = 23 * 60 + 59;
constexpr char MQTT_TOPIC_PREFIX[] = "smartmirror/esp32/smart-clock";
constexpr uint16_t HTTP_PORT = 80;
}  // namespace Config
