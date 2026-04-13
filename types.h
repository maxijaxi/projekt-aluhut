#pragma once

enum class PinMode {
  Input,
  Output
};

struct PinConfig {
  int pin;
  PinMode mode;
};