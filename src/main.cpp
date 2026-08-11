#include <Arduino.h>

constexpr int RELAY_PIN = 23;

struct Control
{
  const char *name;
  int inputPin;
  int outputPin;
  bool previousPressed;
};

Control controls[] = {
    {"TIME", 18, 25, false},
    {"HOUR", 19, 26, false},
    {"MIN", 21, 27, false},
    {"ALARM", 22, 14, false},
    {"SNOOZE", 16, 13, false}, // RX2
    {"ON/OFF", 17, 33, false}  // TX2
};

constexpr int CONTROL_COUNT =
    sizeof(controls) / sizeof(controls[0]);

void setup()
{
  Serial.begin(115200);

  // Keep the clock powered normally.
  digitalWrite(RELAY_PIN, HIGH);
  pinMode(RELAY_PIN, OUTPUT);

  for (int i = 0; i < CONTROL_COUNT; i++)
  {
    // PC817s start released.
    digitalWrite(controls[i].outputPin, LOW);
    pinMode(controls[i].outputPin, OUTPUT);

    // Physical buttons connect their inputs to ESP GND.
    pinMode(controls[i].inputPin, INPUT_PULLUP);
  }

  Serial.println("Button test ready.");
}

void loop()
{
  for (int i = 0; i < CONTROL_COUNT; i++)
  {
    bool pressed =
        digitalRead(controls[i].inputPin) == LOW;

    // Activate the matching PC817.
    digitalWrite(
        controls[i].outputPin,
        pressed ? HIGH : LOW);

    if (pressed != controls[i].previousPressed)
    {
      Serial.print(controls[i].name);
      Serial.println(pressed ? " PRESSED" : " RELEASED");

      controls[i].previousPressed = pressed;
    }
  }

  delay(5);
}