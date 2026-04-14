#include <Arduino.h>
#include "encryption.h"

void xorEncrypt(char* _input, char* _output, byte _key = 0xAA)
{
    
    for (size_t i = 0; _input[i] != '\0'; ++i)
    {
         // XOR Verschlüsselung
         // jeder char wird zusammen mit einem key XOR verglichen (0+0=0, 0+1=1, 1+0=1, 1+1=0) dadurch wird der char verschlüsselt
        _output[i] = _input[i] ^ _key;
    }
    _output[strlen(_input)] = '\0'; // Nullterminator am Ende des Char Arrays hinzufügen
}

