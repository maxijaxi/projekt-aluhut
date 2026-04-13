#pragma once

#include "types.h"

constexpr unsigned baud = 115200;

constexpr PinConfig pins[] = {
    {A0, PinMode::Input},
    {A1, PinMode::Input},
    {2, PinMode::Input}
};