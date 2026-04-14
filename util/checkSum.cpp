#include <Arduino.h>
#include "checkSum.h"

byte checksum(char* _input)
{
    char *charArray = stringToCharArray(_input);
    byte sum = 0;
    // iteriere durch jedes Zeichen im Char Array und addiere den ASCII Wert zum Summe
    // (charArray[i] != '\0') prüft, ob das aktuelle Zeichen nicht der Nullterminator ist, was das Ende des Char Arrays markiert
    for (size_t i = 0; charArray[i] != '\0'; ++i)
    {
        sum += charArray[i];
    }
    delete[] charArray; // gebe den Speicher für das Char Array frei (sonst mem leak lol)
    return sum;
}

// wandel string zu einem array aus char
void *stringToCharArray(char* _input)
{
    // erstelle char Array mit Länge des Strings + 1 für Nullterminator
    // (Nullterminator ist wichtig damit die Funktion weiß, wo das Ende des Char Arrays ist)
    char *charArray = new char[strlen(_input) + 1];
    // kopiere jedes Zeichen des Strings in das Char Array
    // size_t ist eine variable die einem unsigned long ähnlich kommt
    for (size_t i = 0; i < strlen(_input); ++i)
    {
        charArray[i] = _input[i];
    }
    // füge Nullterminator am Ende des Char Arrays hinzu
    // space = "00100000" null terminator ist "00000000", dadurch kein konflikt
    charArray[strlen(_input)] = '\0'; 
    return charArray;
}