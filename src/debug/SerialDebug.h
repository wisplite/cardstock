#pragma once

#include <Arduino.h>

struct PacketHeader;

namespace SerialDebug {
    int handleSerialInput();
    int processPacket(PacketHeader& header, uint8_t* data);
    bool getDebugMode();
}