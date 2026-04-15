#include <Arduino.h>
#include <string.h>

#if defined(HAVE_HWSERIAL1)
#define HM10_SERIAL Serial1
#else
#include <SoftwareSerial.h>
SoftwareSerial bleSerial(4, 5);
#define HM10_SERIAL bleSerial
#endif

// ============================================================
// Konstanten & Buffer
// ============================================================
const int MAX_MSG_SIZE = 64;

char inputBuffer[MAX_MSG_SIZE + 1] = "";
int inputLen = 0;

// ============================================================
// handleInput: Nachricht als Klartext via BLE senden
// ============================================================
void handleInput(char* _input) {
  if (_input == nullptr || _input[0] == '\0') return;
  HM10_SERIAL.println(_input);
}

// ============================================================
// checkBLEReceive: Eingehende BLE-Nachrichten lesen
// und im Serial Monitor ausgeben
// ============================================================
void checkBLEReceive() {
  static char recvBuf[MAX_MSG_SIZE + 2];
  static size_t recvLen = 0;

  while (HM10_SERIAL.available()) {
    int readByte = HM10_SERIAL.read();
    if (readByte == -1) break;
    char c = static_cast<char>(readByte);

        if (c == '\n' || c == '\r') {
      if (recvLen > 0) {
        recvBuf[recvLen] = '\0';
        // HM-10 Modul-Echo ignorieren
        if (strncmp(recvBuf, "OK", 2) != 0 && strncmp(recvBuf, "TX=", 3) != 0) {
          Serial.print("[RX] ");
          Serial.println(recvBuf);
        }
        recvLen = 0;
      }

      continue;
    }

    if (recvLen < MAX_MSG_SIZE) {
      recvBuf[recvLen++] = c;
    }
  }
}

// ============================================================
// setup
// ============================================================
void setup() {
  Serial.begin(9600);
  HM10_SERIAL.begin(9600);
  Serial.println("Chat bereit. Nachricht eingeben und mit Enter senden.");
}

// ============================================================
// loop: Non-blocking Serial-Eingabe + BLE-Empfang
// ============================================================
void loop() {
  checkBLEReceive();

  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputLen > 0) {
        inputBuffer[inputLen] = '\0';
        handleInput(inputBuffer);
        inputLen = 0;
        inputBuffer[0] = '\0';
      }
    } else if (inputLen < MAX_MSG_SIZE) {
      inputBuffer[inputLen++] = c;
    }
  }
}
