#include <Arduino.h>
#include "config.h"

void PinInit() {
  Serial.println("============================Pin Init============================");

  for (const auto& p : pins) {
    String msg = "Pin: " + String(p.pin) + " init as ";

    if (p.mode == PinMode::Input) {
      pinMode(p.pin, INPUT);
      msg += "input";
    } else {
      pinMode(p.pin, OUTPUT);
      msg += "output";
    }

    Serial.println(msg);
  }

  Serial.println("============================Pin Scc============================");
}