#include <Arduino.h>
#include "smart_clock_app.h"

SmartClockApp app;

void setup() {
  app.begin();
}

void loop() {
  app.update();
  delay(1);
}
