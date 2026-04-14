#include <Arduino.h>
#include "checkSum.h"

void makeReadyForSend(char* _data, char* _output) {
    byte _checkSum = checksum(_data);
    sprintf(_output, "%s\x03%d\n", _data, _checkSum);
}