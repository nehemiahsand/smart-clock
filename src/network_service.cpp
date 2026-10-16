#include "network_service.h"

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include "app_config.h"
#include "time_utils.h"

NetworkService* NetworkService::instance_ = nullptr;

NetworkService::NetworkService() : mqtt_(tls_), http_(Config::HTTP_PORT) {
  instance_ = this;
}

void NetworkService::begin(StateProvider provider, CommandHandler handler) {
  stateProvider_ = provider;
  commandHandler_ = handler;
  tls_.setCACert(MQTT_CA_CERT);
  tls_.setTimeout(1000);
  mqtt_.setServer(MQTT_HOST, MQTT_PORT);
  mqtt_.setCallback(mqttThunk);
  mqtt_.setBufferSize(4096);
  mqtt_.setSocketTimeout(1);
}

bool NetworkService::authorized() {
  if (strlen(HTTP_BEARER_TOKEN) < 16) return false;
  return http_.hasHeader("Authorization") &&
         http_.header("Authorization") ==
             String("Bearer ") + HTTP_BEARER_TOKEN;
}

void NetworkService::sendJson(int code, const String& body) {
  http_.send(code, "application/json", body);
}

void NetworkService::startLocalServices() {
  if (servicesStarted_) return;
  servicesStarted_ = true;
  MDNS.begin(Config::HOSTNAME);
  const char* headers[] = {"Authorization"};
  http_.collectHeaders(headers, 1);
  http_.on("/api/v1/health", HTTP_GET, [this]() {
    if (!authorized()) return sendJson(401, "{\"error\":\"unauthorized\"}");
    sendJson(200, stateProvider_ ? stateProvider_() : "{}");
  });
  http_.on("/api/v1/state", HTTP_GET, [this]() {
    if (!authorized()) return sendJson(401, "{\"error\":\"unauthorized\"}");
    sendJson(200, stateProvider_ ? stateProvider_() : "{}");
  });
  http_.on("/api/v1/command", HTTP_POST, [this]() {
    if (!authorized()) return sendJson(401, "{\"error\":\"unauthorized\"}");
    String response;
    if (!commandHandler_ || !commandHandler_(http_.arg("plain"), response))
      return sendJson(400, response.length() ? response : "{\"error\":\"invalid command\"}");
    sendJson(200, response);
  });
  http_.onNotFound([this]() { sendJson(404, "{\"error\":\"not found\"}"); });
  http_.begin();

  if (strlen(OTA_PASSWORD) >= 12) {
    ArduinoOTA.setHostname(Config::HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);
    ArduinoOTA.begin();
  }
}

void NetworkService::mqttThunk(char* topic, uint8_t* payload, unsigned int length) {
  if (instance_) instance_->onMqtt(topic, payload, length);
}

void NetworkService::onMqtt(char*, uint8_t* payload, unsigned int length) {
  if (length == 0 || length >= 4096) return;
  String request;
  request.reserve(length);
  for (unsigned int i = 0; i < length; ++i)
    request += static_cast<char>(payload[i]);
  String response;
  const bool ok = commandHandler_ && commandHandler_(request, response);
  if (!response.length())
    response = ok ? "{\"ok\":true}" : "{\"ok\":false}";
  mqtt_.publish((String(Config::MQTT_TOPIC_PREFIX) + "/response").c_str(),
                response.c_str(), false);
}

void NetworkService::connectMqtt(uint32_t nowMs) {
  if (mqtt_.connected() ||
      !TimeUtils::elapsed(nowMs, lastMqttAttemptMs_, Config::MQTT_RETRY_MS))
    return;
  lastMqttAttemptMs_ = nowMs;
  const String statusTopic = String(Config::MQTT_TOPIC_PREFIX) + "/status";
  if (mqtt_.connect(Config::DEVICE_ID, MQTT_USERNAME, MQTT_PASSWORD,
                    statusTopic.c_str(), 0, true, "offline")) {
    mqtt_.subscribe((String(Config::MQTT_TOPIC_PREFIX) + "/command").c_str());
    mqtt_.publish(statusTopic.c_str(), "online", true);
    publishState();
  }
}

void NetworkService::update(uint32_t nowMs, bool timeValid) {
  if (WiFi.status() != WL_CONNECTED) return;
  startLocalServices();
  http_.handleClient();
  if (strlen(OTA_PASSWORD) >= 12) ArduinoOTA.handle();
  if (timeValid && strlen(MQTT_CA_CERT) > 100 && strlen(MQTT_USERNAME) > 0) {
    connectMqtt(nowMs);
    mqtt_.loop();
    if (mqtt_.connected() &&
        TimeUtils::elapsed(nowMs, lastStatePublishMs_, Config::STATE_PUBLISH_MS))
      publishState();
  }
}

bool NetworkService::publishState() {
  if (!mqtt_.connected() || !stateProvider_) return false;
  const String state = stateProvider_();
  lastStatePublishMs_ = millis();
  return mqtt_.publish((String(Config::MQTT_TOPIC_PREFIX) + "/state").c_str(),
                       state.c_str(), true);
}

bool NetworkService::publishEvent(const char* type, const String& payload) {
  if (!mqtt_.connected()) return false;
  JsonDocument document;
  document["type"] = type;
  document["deviceId"] = Config::DEVICE_ID;
  document["timestamp"] = static_cast<int64_t>(time(nullptr)) * 1000;
  JsonObject data = document["payload"].to<JsonObject>();
  data["raw"] = serialized(payload);
  String output;
  serializeJson(document, output);
  return mqtt_.publish((String(Config::MQTT_TOPIC_PREFIX) + "/event").c_str(),
                       output.c_str(), false);
}
