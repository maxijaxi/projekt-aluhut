#pragma once

#include "types.h"

constexpr unsigned BAUD = 115200;

constexpr PinConfig pins[] = {
    {0xA0, PinMode::Input},
    {0xA1, PinMode::Input},
    {2, PinMode::Input}
};