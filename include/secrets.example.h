#pragma once

// Copy to include/secrets.h and replace every placeholder.
// Never commit include/secrets.h.
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#define MQTT_HOST "nehemiah-pi5"
#define MQTT_PORT 8883
#define MQTT_USERNAME "YOUR_MQTT_USERNAME"
#define MQTT_PASSWORD "YOUR_MQTT_PASSWORD"
#define MQTT_CA_CERT R"PEM(-----BEGIN CERTIFICATE-----
YOUR_CA_CERTIFICATE
-----END CERTIFICATE-----
)PEM"
#define HTTP_BEARER_TOKEN "GENERATE_A_LONG_RANDOM_TOKEN"
#define OTA_PASSWORD "GENERATE_A_DIFFERENT_LONG_RANDOM_PASSWORD"
