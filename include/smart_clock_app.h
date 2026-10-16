#pragma once
#include <Arduino.h>
#include "control_panel.h"
#include "network_service.h"
#include "persistent_store.h"
#include "sharp_controller.h"
#include "time_service.h"

class SmartClockApp {
 public:
  void begin();
  void update();

 private:
  ControlPanel panel_;
  SharpController sharp_;
  TimeService time_;
  NetworkService network_;
  PersistentStore store_;
  PersistedData data_{};

  bool masterEnabled_ = false;
  bool masterStateInitialized_ = false;
  bool controlsUnlocked_ = false;
  bool needsProgramming_ = true;
  bool sharpWasBusy_ = false;
  bool ringing_ = false;
  uint32_t bootMs_ = 0;
  uint32_t ringStartedMs_ = 0;
  int lastDailySyncYday_ = -1;
  Schedule::Occurrence next_{};

  bool manualEditActive_ = false;
  bool manualEditPending_ = false;
  uint8_t manualHour_ = 0;
  uint8_t manualMinute_ = 0;
  uint32_t manualLastEditMs_ = 0;

  String terminalLine_;
  String recentRequestIds_[8];
  String recentResponses_[8];
  uint8_t recentRequestCursor_ = 0;

  static SmartClockApp* instance_;
  static String stateThunk();
  static bool commandThunk(const String& request, String& response);

  void handleInputs(uint32_t nowMs);
  void handleInput(const InputEvent& event, uint32_t nowMs);
  void updateSchedule(uint32_t nowMs);
  void updateManualImport(uint32_t nowMs);
  void setMaster(bool enabled, time_t now, const char* source);
  void recomputeNext(time_t now);
  bool startProgramming(bool forceTimeSync);
  void startRinging(time_t now, uint32_t nowMs);
  void finishRinging(bool snooze, time_t now);
  bool addPhysicalAlarm(time_t now);
  bool addSnoozeAlarm(time_t now);
  void markDirty();
  String buildStateJson();
  bool handleCommand(const String& request, String& response);
  void updateTerminal();
  void processSerial(String command);
  void printStatus();
};
