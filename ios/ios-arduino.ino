#include <Arduino.h>
#include <string.h>
#include <SoftwareSerial.h>

// ============================================================
// Pins / Serial Settings (UNO: SoftwareSerial)
// ============================================================
static const int BLE_RX_PIN = 4;   // Arduino empfängt hier (an HM-10 TXD)
static const int BLE_TX_PIN = 5;   // Arduino sendet hier (an HM-10 RXD)
static const long    BLE_BAUD   = 9600;

static const long    USB_BAUD   = 9600;

// ============================================================
// Konstanten & Buffer
// ============================================================
static const size_t MAX_MSG_SIZE = 64;

// Outgoing (USB -> BLE)
static char   inputBuffer[MAX_MSG_SIZE + 1] = {0};
static size_t inputLen = 0;

// Incoming (BLE -> USB)
static char   recvBuffer[MAX_MSG_SIZE + 1] = {0};
static size_t recvLen = 0;

// Software serial to HM-10
static SoftwareSerial bleSerial(BLE_RX_PIN, BLE_TX_PIN); // (RX, TX)

// Einheitlicher Name, falls du später leicht auf HW-Serial umbauen willst
#define HM10_SERIAL bleSerial

// ============================================================
// Senden: Nachricht als Klartext via BLE senden
// ============================================================
static void sendLineToBLE(const char* line) {
  if (!line || line[0] == '\0') return;
  HM10_SERIAL.println(line);
}

// ============================================================
// Empfang: Eingehende BLE-Nachrichten lesen und auf USB ausgeben
// ============================================================
static void pollBLEReceive() {
  while (HM10_SERIAL.available() > 0) {
    int b = HM10_SERIAL.read();
    if (b < 0) break;

    char c = (char)b;

    // Zeilenende erkannt -> Buffer ausgeben
    if (c == '\n' || c == '\r') {
      if (recvLen > 0) {
        recvBuffer[recvLen] = '\0';

        // HM-10 Modul-Echo/Statuszeilen ignorieren
        if (strncmp(recvBuffer, "OK", 2) != 0 &&
            strncmp(recvBuffer, "TX=", 3) != 0) {
          Serial.print("[RX] ");
          Serial.println(recvBuffer);
        }

        recvLen = 0; // reset für nächste Zeile
      }
      continue;
    }

    // Normales Zeichen puffern (Überlauf wird einfach abgeschnitten)
    if (recvLen < MAX_MSG_SIZE) {
      recvBuffer[recvLen++] = c;
    }
  }
}

// ============================================================
// USB-Serial Eingabe (non-blocking) -> bei Zeilenende senden
// ============================================================
static void pollUSBInputAndSend() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();

    if (c == '\n' || c == '\r') {
      if (inputLen > 0) {
        inputBuffer[inputLen] = '\0';
        sendLineToBLE(inputBuffer);

        // Reset
        inputLen = 0;
        inputBuffer[0] = '\0';
      }
      continue;
    }

    if (inputLen < MAX_MSG_SIZE) {
      inputBuffer[inputLen++] = c;
    }
  }
}

// ============================================================
// setup / loop
// ============================================================
void setup() {
  Serial.begin(USB_BAUD);
  HM10_SERIAL.begin(BLE_BAUD);

  Serial.println("Chat bereit (Arduino UNO + HM-10).");
  Serial.println("Im Serial Monitor eine Zeile tippen und Enter zum Senden.");
}

void loop() {
  pollBLEReceive();
  pollUSBInputAndSend();
}
