#pragma once
#include <Arduino.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <WiFiClientSecure.h>

class NetworkService {
 public:
  using StateProvider = String (*)();
  using CommandHandler = bool (*)(const String& request, String& response);

  NetworkService();
  void begin(StateProvider stateProvider, CommandHandler commandHandler);
  void update(uint32_t nowMs, bool timeValid);
  bool mqttConnected() { return mqtt_.connected(); }
  bool publishState();
  bool publishEvent(const char* type, const String& payload = "{}");

 private:
  WiFiClientSecure tls_;
  PubSubClient mqtt_;
  WebServer http_;
  StateProvider stateProvider_ = nullptr;
  CommandHandler commandHandler_ = nullptr;
  uint32_t lastMqttAttemptMs_ = 0;
  uint32_t lastStatePublishMs_ = 0;
  bool servicesStarted_ = false;

  static NetworkService* instance_;
  static void mqttThunk(char* topic, uint8_t* payload, unsigned int length);
  void onMqtt(char* topic, uint8_t* payload, unsigned int length);
  void startLocalServices();
  void connectMqtt(uint32_t nowMs);
  bool authorized();
  void sendJson(int code, const String& body);
};
