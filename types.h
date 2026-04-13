#pragma once
#include <Arduino.h>

enum class PinMode {
  Input,
  Output
};

struct PinConfig {
  int pin;
  PinMode mode;
};