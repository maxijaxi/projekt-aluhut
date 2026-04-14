#pragma once
#include <Arduino.h>

#include "types.h"

constexpr unsigned BAUD = 115200;

constexpr PinConfig pins[] = {
    {0xA0, PinMode::Input},
    {0xA1, PinMode::Input},
    {0x2, PinMode::Input}
};

const int MAX_MSG_SIZE = 64;
const int MAX_PACKET_SIZE = MAX_MSG_SIZE + 10; // +10 für ETX + checksum + \n