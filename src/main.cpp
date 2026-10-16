#include <Arduino.h>
#include <WiFi.h>
#include <time.h>

#include "secrets.h"
#include <sys/time.h>

// ---------- Pins ----------

constexpr int RELAY_PIN = 23;

constexpr int TIME_OUTPUT = 25;
constexpr int HOUR_OUTPUT = 26;
constexpr int MIN_OUTPUT = 27;

struct Control
{
  const char *commandName;
  const char *displayName;
  int inputPin;
  int outputPin;
  bool previousPhysicalState;
  bool terminalHeld;
};

Control controls[] = {
    {"time", "TIME", 18, TIME_OUTPUT, false, false},
    {"hour", "HOUR", 19, HOUR_OUTPUT, false, false},
    {"min", "MIN", 21, MIN_OUTPUT, false, false},
    {"alarm", "ALARM", 22, 14, false, false},
    {"snooze", "SNOOZE", 16, 13, false, false},
    {"onoff", "ON/OFF", 17, 33, false, false}};

constexpr int CONTROL_COUNT =
    sizeof(controls) / sizeof(controls[0]);

// ---------- Clock configuration ----------

// How long each simulated button press lasts.
constexpr unsigned long PRESS_TIME_MS = 250;

// How long to wait between presses.
constexpr unsigned long RELEASE_TIME_MS = 150;

// How long to remove power during a reset.
constexpr unsigned long RESET_TIME_MS = 10000;

// How long to wait for the Sharp clock to restart.
constexpr unsigned long CLOCK_BOOT_TIME_MS = 3000;

// How long TIME is held before pressing HOUR or MIN.
constexpr unsigned long MODE_START_MS = 600;

// How long TIME remains held after the final press.
constexpr unsigned long MODE_END_MS = 300;

// How long before the target time to restore power.
constexpr unsigned long FINAL_WAIT_MARGIN_MS = 3000;

// How long to wait after the final press before the clock is expected to be synchronized.
constexpr long FINAL_SYNC_ADJUSTMENT_MS = 0;

// Assumes the Sharp clock resets to 12:00 AM.
constexpr int RESET_HOUR_24 = 0;

// Central Time with automatic daylight-saving changes.
constexpr char TIME_ZONE[] =
    "CST6CDT,M3.2.0,M11.1.0";

String terminalLine;

// ---------- Control helpers ----------

int findControl(const String &requestedName)
{
  String name = requestedName;
  name.trim();
  name.toLowerCase();

  if (name == "on/off")
  {
    name = "onoff";
  }

  for (int i = 0; i < CONTROL_COUNT; i++)
  {
    if (name == controls[i].commandName)
    {
      return i;
    }
  }

  return -1;
}

void applyControlOutputs()
{
  for (int i = 0; i < CONTROL_COUNT; i++)
  {
    bool physicallyPressed =
        digitalRead(controls[i].inputPin) == LOW;

    bool shouldActivate =
        physicallyPressed || controls[i].terminalHeld;

    // HIGH turns on the PC817 and presses the clock button.
    digitalWrite(
        controls[i].outputPin,
        shouldActivate ? HIGH : LOW);
  }
}

void releaseAllControls()
{
  for (int i = 0; i < CONTROL_COUNT; i++)
  {
    controls[i].terminalHeld = false;
    digitalWrite(controls[i].outputPin, LOW);
  }
}

void tapControl(int index)
{
  Serial.print("Tapping ");
  Serial.println(controls[index].displayName);

  bool previouslyHeld = controls[index].terminalHeld;

  controls[index].terminalHeld = true;
  applyControlOutputs();
  delay(PRESS_TIME_MS);

  controls[index].terminalHeld = previouslyHeld;
  applyControlOutputs();
  delay(RELEASE_TIME_MS);
}

void holdControl(int index)
{
  controls[index].terminalHeld = true;
  applyControlOutputs();

  Serial.print("Holding ");
  Serial.println(controls[index].displayName);
}

void releaseControl(int index)
{
  controls[index].terminalHeld = false;
  applyControlOutputs();

  Serial.print("Released ");
  Serial.println(controls[index].displayName);
}

void updatePhysicalButtons()
{
  for (int i = 0; i < CONTROL_COUNT; i++)
  {
    bool pressed =
        digitalRead(controls[i].inputPin) == LOW;

    if (pressed != controls[i].previousPhysicalState)
    {
      Serial.print("Physical ");
      Serial.print(controls[i].displayName);
      Serial.println(
          pressed ? " PRESSED" : " RELEASED");

      controls[i].previousPhysicalState = pressed;
    }
  }

  applyControlOutputs();
}

// ---------- Wi-Fi and network time ----------

bool connectToWiFi()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    return true;
  }

  Serial.print("Connecting to Wi-Fi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long started = millis();

  while (
      WiFi.status() != WL_CONNECTED &&
      millis() - started < 20000)
  {
    Serial.print(".");
    delay(500);
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED)
  {
    Serial.println("Wi-Fi connection failed.");
    return false;
  }

  Serial.print("Connected. IP: ");
  Serial.println(WiFi.localIP());

  return true;
}

bool obtainNetworkTime(tm &localTime)
{
  if (!connectToWiFi())
  {
    return false;
  }

  configTzTime(
      TIME_ZONE,
      "pool.ntp.org",
      "time.nist.gov");

  Serial.println("Obtaining network time...");
  delay(3000);

  if (!getLocalTime(&localTime, 15000))
  {
    Serial.println("Network time failed.");
    return false;
  }

  return true;
}

// ---------- Clock reset and setting ----------

void resetSharpClock()
{
  Serial.println("Resetting Sharp clock...");

  releaseAllControls();

  // The relay is active-low.
  // LOW activates the relay and disconnects clock power.
  digitalWrite(RELAY_PIN, LOW);
  delay(RESET_TIME_MS);

  // HIGH releases the relay and restores clock power.
  digitalWrite(RELAY_PIN, HIGH);
  delay(CLOCK_BOOT_TIME_MS);

  Serial.println("Sharp clock power restored.");
}

void pulseClockOutput(int outputPin)
{
  digitalWrite(outputPin, HIGH);
  delay(PRESS_TIME_MS);

  digitalWrite(outputPin, LOW);
  delay(RELEASE_TIME_MS);
}

void setSharpClock(int targetHour24, int targetMinute)
{
  int hourPresses =
      (targetHour24 - RESET_HOUR_24 + 24) % 24;

  Serial.print("Setting time with ");
  Serial.print(hourPresses);
  Serial.print(" hour presses and ");
  Serial.print(targetMinute);
  Serial.println(" minute presses.");

  // Hold the Sharp clock's TIME button.
  digitalWrite(TIME_OUTPUT, HIGH);
  delay(MODE_START_MS);

  // TIME remains held while HOUR is pressed.
  for (int i = 0; i < hourPresses; i++)
  {
    pulseClockOutput(HOUR_OUTPUT);
  }

  // TIME remains held while MIN is pressed.
  for (int i = 0; i < targetMinute; i++)
  {
    pulseClockOutput(MIN_OUTPUT);
  }

  delay(MODE_END_MS);

  // Release TIME.
  digitalWrite(TIME_OUTPUT, LOW);

  Serial.println("Time-setting sequence finished.");
}

unsigned long estimateSettingTimeMs(
    int targetHour24,
    int targetMinute)
{
  int hourPresses =
      (targetHour24 - RESET_HOUR_24 + 24) % 24;

  int totalPresses =
      hourPresses + targetMinute;

  return MODE_START_MS +
         MODE_END_MS +
         totalPresses *
             (PRESS_TIME_MS + RELEASE_TIME_MS);
}

int64_t currentEpochMilliseconds()
{
  timeval currentTime;
  gettimeofday(&currentTime, nullptr);

  return static_cast<int64_t>(currentTime.tv_sec) * 1000 +
         currentTime.tv_usec / 1000;
}

void waitUntilEpochMilliseconds(int64_t targetTime)
{
  while (true)
  {
    int64_t remaining =
        targetTime - currentEpochMilliseconds();

    if (remaining <= 0)
    {
      return;
    }

    if (remaining > 1000)
    {
      delay(static_cast<unsigned long>(remaining - 500));
    }
    else
    {
      // Short waits keep the final button press accurate.
      delay(1);
    }
  }
}

void autoSetSharpClock()
{
  tm networkTime;

  if (!obtainNetworkTime(networkTime))
  {
    Serial.println("Auto-set cancelled.");
    return;
  }

  Serial.println("Beginning precise clock synchronization.");

  releaseAllControls();

  // Turn the Sharp clock off and fully reset it.
  digitalWrite(RELAY_PIN, LOW);
  delay(RESET_TIME_MS);

  int64_t nowMs = currentEpochMilliseconds();

  // Begin with the next real minute boundary.
  int64_t targetBoundaryMs =
      ((nowMs / 60000) + 1) * 60000;

  int targetHour = 0;
  int targetMinute = 0;
  int minuteBeforeTarget = 0;
  int hourPresses = 0;
  int minutePresses = 0;
  unsigned long setupDurationMs = 0;
  int64_t plannedRestoreMs = 0;

  while (true)
  {
    time_t targetEpoch =
        static_cast<time_t>(targetBoundaryMs / 1000);

    tm targetTime;
    localtime_r(&targetEpoch, &targetTime);

    targetHour = targetTime.tm_hour;
    targetMinute = targetTime.tm_min;

    // The final MIN press will move from this minute
    // to the desired minute.
    minuteBeforeTarget =
        (targetMinute + 59) % 60;

    hourPresses =
        (targetHour - RESET_HOUR_24 + 24) % 24;

    minutePresses = minuteBeforeTarget;

    setupDurationMs =
        CLOCK_BOOT_TIME_MS +
        MODE_START_MS +
        static_cast<unsigned long>(
            hourPresses + minutePresses) *
            (PRESS_TIME_MS + RELEASE_TIME_MS);

    // Restore power late enough that setup finishes only
    // a few seconds before the target boundary.
    plannedRestoreMs =
        targetBoundaryMs -
        setupDurationMs -
        FINAL_WAIT_MARGIN_MS;

    if (plannedRestoreMs >
        currentEpochMilliseconds() + 250)
    {
      break;
    }

    // There isn't enough time, so use the following minute.
    targetBoundaryMs += 60000;
  }

  Serial.printf(
      "Target time: %02d:%02d:00\n",
      targetHour,
      targetMinute);

  Serial.printf(
      "Preparing %d hour presses and %d early minute presses.\n",
      hourPresses,
      minutePresses);

  Serial.println("Waiting to restore clock power...");

  // Leave the clock powered off until the calculated start time.
  waitUntilEpochMilliseconds(plannedRestoreMs);

  digitalWrite(RELAY_PIN, HIGH);
  delay(CLOCK_BOOT_TIME_MS);

  // Hold TIME during the entire setting operation.
  digitalWrite(TIME_OUTPUT, HIGH);
  delay(MODE_START_MS);

  for (int i = 0; i < hourPresses; i++)
  {
    pulseClockOutput(HOUR_OUTPUT);
  }

  // Set the display to one minute before the target.
  for (int i = 0; i < minutePresses; i++)
  {
    pulseClockOutput(MIN_OUTPUT);
  }

  if (currentEpochMilliseconds() >= targetBoundaryMs)
  {
    digitalWrite(TIME_OUTPUT, LOW);

    Serial.println(
        "Setup took too long. Please run set again.");
    return;
  }

  Serial.println(
      "Waiting for the exact minute boundary...");

  waitUntilEpochMilliseconds(targetBoundaryMs + FINAL_SYNC_ADJUSTMENT_MS);

  // This final press changes to the correct minute and
  // resets the Sharp clock's hidden seconds to zero.
  digitalWrite(MIN_OUTPUT, HIGH);
  delay(PRESS_TIME_MS);

  digitalWrite(MIN_OUTPUT, LOW);
  delay(RELEASE_TIME_MS);

  delay(MODE_END_MS);
  digitalWrite(TIME_OUTPUT, LOW);

  Serial.printf(
      "Clock synchronized to %02d:%02d:00.\n",
      targetHour,
      targetMinute);
}

// ---------- Terminal commands ----------

void printHelp()
{
  Serial.println();
  Serial.println("Available commands:");
  Serial.println("  time");
  Serial.println("  hour");
  Serial.println("  min");
  Serial.println("  alarm");
  Serial.println("  snooze");
  Serial.println("  onoff");
  Serial.println("  tap <button>");
  Serial.println("  hold <button>");
  Serial.println("  release <button>");
  Serial.println("  set");
  Serial.println("  reset");
  Serial.println("  help");
  Serial.println();
  Serial.println("Example:");
  Serial.println("  hold time");
  Serial.println("  hour");
  Serial.println("  release time");
  Serial.println();
}

void processCommand(String command)
{
  command.trim();
  command.toLowerCase();

  if (command.length() == 0)
  {
    return;
  }

  Serial.print("> ");
  Serial.println(command);

  if (command == "help")
  {
    printHelp();
    return;
  }

  if (command == "set" || command == "sync")
  {
    autoSetSharpClock();
    return;
  }

  if (command == "reset")
  {
    resetSharpClock();
    return;
  }

  // A button name by itself means tap.
  int directControl = findControl(command);

  if (directControl >= 0)
  {
    tapControl(directControl);
    return;
  }

  int firstSpace = command.indexOf(' ');

  if (firstSpace < 0)
  {
    Serial.println("Unknown command. Enter help.");
    return;
  }

  String action = command.substring(0, firstSpace);
  String buttonName = command.substring(firstSpace + 1);

  int controlIndex = findControl(buttonName);

  if (controlIndex < 0)
  {
    Serial.println("Unknown button name.");
    return;
  }

  if (action == "tap" || action == "press")
  {
    tapControl(controlIndex);
  }
  else if (action == "hold")
  {
    holdControl(controlIndex);
  }
  else if (action == "release")
  {
    releaseControl(controlIndex);
  }
  else
  {
    Serial.println("Unknown action. Enter help.");
  }
}

void updateTerminal()
{
  while (Serial.available())
  {
    char incoming = Serial.read();

    if (incoming == '\r' || incoming == '\n')
    {
      if (terminalLine.length() > 0)
      {
        processCommand(terminalLine);
        terminalLine = "";
      }
    }
    else if (terminalLine.length() < 80)
    {
      terminalLine += incoming;
    }
  }
}

// ---------- Setup and loop ----------

void setup()
{
  Serial.begin(115200);

  // Set the relay HIGH before enabling it as an output.
  // This keeps clock power connected during ESP32 startup.
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(RELAY_PIN, OUTPUT);

  for (int i = 0; i < CONTROL_COUNT; i++)
  {
    // Start every PC817 with its button released.
    digitalWrite(controls[i].outputPin, LOW);
    pinMode(controls[i].outputPin, OUTPUT);

    // The physical buttons connect their input to ground.
    pinMode(
        controls[i].inputPin,
        INPUT_PULLUP);
  }

  Serial.println();
  Serial.println("Smart clock controller ready.");
  printHelp();

  connectToWiFi();
}

void loop()
{
  updatePhysicalButtons();
  updateTerminal();

  delay(5);
}