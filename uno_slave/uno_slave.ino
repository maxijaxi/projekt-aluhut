// =============================================================
// PROJEKT ALUHUT – Arduino Uno Slave
// Hardware : Arduino Uno + HM-10 BT-Modul
//            BT  RX=7  TX=8
//            Joystick X=A0  Y=A1  SW=4
//            RFID RC522  SS=10  RST=9
//            OLED SH1106 128x64 via I2C (A4=SDA, A5=SCL)
//
// Protokoll (Senden & Empfangen):
//   <hex-verschlüsselte Daten>\x03<Prüfsumme dez>\n
//   – Daten werden mit XOR(0xAA) verschlüsselt und als Hex
//     kodiert, damit keine Steuerzeichen im Datenstrom entstehen.
//   – Prüfsumme wird über den Klartext berechnet.
// =============================================================

#include <SoftwareSerial.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <string.h>
#include <stdlib.h>

// ----------------------------------------------------------
// BT: HM-10 über SoftwareSerial
// Pin 7 = RX (Uno empfängt, HM-10 TX)
// Pin 8 = TX (Uno sendet,   HM-10 RX)
// ----------------------------------------------------------
SoftwareSerial bt(7, 8);

// ----------------------------------------------------------
// DISPLAY
// ----------------------------------------------------------
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ----------------------------------------------------------
// PINS
// ----------------------------------------------------------
const uint8_t pinX  = A0;
const uint8_t pinY  = A1;
const uint8_t pinSW = 4;   // Joystick-Taster

const uint8_t SS_PIN  = 10;
const uint8_t RST_PIN = 9;
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ----------------------------------------------------------
// RFID-WHITELIST  >>>  ERLAUBTE KARTEN HIER EINTRAGEN  <<<
// ----------------------------------------------------------
const uint8_t AUTH_UID_LENGTH = 4;
const uint8_t authorizedCards[][AUTH_UID_LENGTH] = {
    {0x2D, 0x40, 0x98, 0x38}
};
const uint8_t AUTH_CARD_COUNT =
    sizeof(authorizedCards) / sizeof(authorizedCards[0]);

// ----------------------------------------------------------
// LOCKSCREEN
// ----------------------------------------------------------
bool isUnlocked = false;

enum AuthFeedback { AUTH_IDLE, AUTH_OK, AUTH_FAIL };
AuthFeedback authState = AUTH_IDLE;
unsigned long authFeedbackStart    = 0;
const unsigned long authFeedbackMs = 2000;

// ----------------------------------------------------------
// APP-MODI
// ----------------------------------------------------------
enum Mode { MENU, KEYBOARD, INBOX };
Mode currentMode = MENU;

// ----------------------------------------------------------
// MENÜ / CURSOR
// ----------------------------------------------------------
uint8_t menuSelection = 0;
uint8_t cursorX = 0;
uint8_t cursorY = 0;

// Tastatur-Layout:  '<' = Backspace   '#' = Senden
const char keys[3][10] = {
    {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'},
    {'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T'},
    {'U', 'V', 'W', 'X', 'Y', 'Z', '1', '2', '<', '#'}
};

// ----------------------------------------------------------
// PUFFER & INBOX
// ----------------------------------------------------------
const int MAX_MSG_SIZE    = 16;
const int MAX_PACKET_SIZE = MAX_MSG_SIZE * 2 + 10; // hex + ETX + CS + \n

char    inputBuffer[MAX_MSG_SIZE + 1] = "";
int     inputLen = 0;

const uint8_t MAX_MESSAGES = 2;
char    messages[MAX_MESSAGES][MAX_MSG_SIZE + 1];
uint8_t messageCount   = 0;
uint8_t inboxSelection = 0;
uint8_t inboxScroll    = 0;

// ----------------------------------------------------------
// BT-Empfangspuffer
// ----------------------------------------------------------
char    btBuf[MAX_PACKET_SIZE + 1];
uint8_t btBufLen = 0;

// ----------------------------------------------------------
// DEBOUNCE & TIMING
// ----------------------------------------------------------
bool    lastReading      = HIGH;
bool    stableButton     = HIGH;
bool    lastStableButton = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceMs = 20;

unsigned long lastNavTime = 0;
const unsigned long navMs = 180;

// ==========================================================
// UTIL: PRÜFSUMME (Summe über Klartext-Bytes)
// ==========================================================
byte calcChecksum(const char *s) {
    byte sum = 0;
    for (size_t i = 0; s[i] != '\0'; ++i) {
        sum += (byte)s[i];
    }
    return sum;
}

// ==========================================================
// UTIL: XOR-VERSCHLÜSSELUNG  (symmetrisch → gleiche Funktion
//       zum Entschlüsseln)
//       Ausgabe als Hex-String, damit keine Steuerzeichen
//       im seriellen Datenstrom erscheinen.
// ==========================================================
const byte XOR_KEY = 0xAA;

void xorEncryptToHex(const char *input, char *hexOut) {
    size_t len = strlen(input);
    for (size_t i = 0; i < len; i++) {
        sprintf(hexOut + i * 2, "%02X", (byte)(input[i] ^ XOR_KEY));
    }
    hexOut[len * 2] = '\0';
}

// Hex-String entschlüsseln → Klartext
bool hexToXorDecrypt(const char *hexIn, char *out) {
    size_t hexLen = strlen(hexIn);
    if (hexLen % 2 != 0) return false;
    size_t len = hexLen / 2;
    if (len > (size_t)MAX_MSG_SIZE) return false;
    for (size_t i = 0; i < len; i++) {
        char byteStr[3] = {hexIn[i * 2], hexIn[i * 2 + 1], '\0'};
        out[i] = (char)((byte)strtol(byteStr, NULL, 16) ^ XOR_KEY);
    }
    out[len] = '\0';
    return true;
}

// ==========================================================
// UTIL: PAKET ZUSAMMENSTELLEN (makeReadyForSend)
// Format: <hex_encrypted>\x03<checksum_dez>\n
// _output muss mindestens MAX_PACKET_SIZE+1 Bytes groß sein.
// ==========================================================
void makeReadyForSend(const char *data, char *output) {
    char hexBuf[MAX_MSG_SIZE * 2 + 1];
    byte cs = calcChecksum(data);
    xorEncryptToHex(data, hexBuf);
    sprintf(output, "%s\x03%u\n", hexBuf, (unsigned int)cs);
}

// ==========================================================
// UTIL: EINGEHENDES PAKET PARSEN
// Gibt true zurück, dekodierter Klartext landet in _out.
// ==========================================================
bool parseIncoming(const char *raw, char *out) {
    const char *etx = strchr(raw, '\x03');
    if (!etx) return false;

    size_t hexLen = (size_t)(etx - raw);
    if (hexLen == 0 || hexLen > (size_t)(MAX_MSG_SIZE * 2)) return false;

    char hexBuf[MAX_MSG_SIZE * 2 + 1];
    strncpy(hexBuf, raw, hexLen);
    hexBuf[hexLen] = '\0';

    if (!hexToXorDecrypt(hexBuf, out)) return false;

    byte receivedCs = (byte)atoi(etx + 1);
    byte calcCs     = calcChecksum(out);
    if (receivedCs != calcCs) {
        Serial.println("[WARN] Checksum-Fehler!");
    }
    return true;
}

// ==========================================================
// INBOX: NACHRICHT SPEICHERN + SERIAL-LOG
// ==========================================================
void saveMessageToInbox(const char *msg) {
    if (!msg || msg[0] == '\0') return;

    Serial.print("[MSG] ");
    Serial.println(msg);

    if (messageCount < MAX_MESSAGES) {
        strncpy(messages[messageCount], msg, MAX_MSG_SIZE);
        messages[messageCount][MAX_MSG_SIZE] = '\0';
        messageCount++;
    } else {
        // Älteste Nachricht verwerfen, restliche verschieben
        for (uint8_t i = 0; i < MAX_MESSAGES - 1; i++) {
            strncpy(messages[i], messages[i + 1], MAX_MSG_SIZE + 1);
        }
        strncpy(messages[MAX_MESSAGES - 1], msg, MAX_MSG_SIZE);
        messages[MAX_MESSAGES - 1][MAX_MSG_SIZE] = '\0';
    }

    inboxSelection = (messageCount > 0) ? messageCount - 1 : 0;
    inboxScroll    = (inboxSelection >= 2) ? (inboxSelection - 2) : 0;
}

// ==========================================================
// BT SENDEN
// ==========================================================
void handleInput(const char *input) {
    char packet[MAX_PACKET_SIZE + 1];
    makeReadyForSend(input, packet);
    bt.print(packet);
    Serial.print("[SEND] ");
    Serial.println(packet);
}

// ==========================================================
// BT EMPFANGEN  (Polling in loop)
// ==========================================================
void readBtIncoming() {
    while (bt.available()) {
        char c = (char)bt.read();
        if (c == '\n') {
            btBuf[btBufLen] = '\0';
            if (btBufLen > 0) {
                char decoded[MAX_MSG_SIZE + 1];
                if (parseIncoming(btBuf, decoded)) {
                    saveMessageToInbox(decoded);
                }
                btBufLen = 0;
            }
        } else if (btBufLen < (uint8_t)MAX_PACKET_SIZE) {
            btBuf[btBufLen++] = c;
        }
    }
}

// ==========================================================
// BUTTON-DEBOUNCE
// ==========================================================
bool updatePressedEvent() {
    bool reading = digitalRead(pinSW);
    if (reading != lastReading) {
        lastDebounceTime = millis();
        lastReading = reading;
    }
    if (millis() - lastDebounceTime > debounceMs) {
        stableButton = reading;
    }
    bool pressed = (lastStableButton == HIGH && stableButton == LOW);
    lastStableButton = stableButton;
    return pressed;
}

// ==========================================================
// INPUT-PUFFER
// ==========================================================
void appendChar(char c) {
    if (inputLen < MAX_MSG_SIZE) {
        inputBuffer[inputLen++] = c;
        inputBuffer[inputLen]   = '\0';
    }
}

void backspaceChar() {
    if (inputLen > 0) inputBuffer[--inputLen] = '\0';
}

void clearInput() {
    inputLen      = 0;
    inputBuffer[0] = '\0';
}

// ==========================================================
// RFID
// ==========================================================
bool isAuthorizedUID(byte *uid, byte uidSize) {
    if (uidSize != AUTH_UID_LENGTH) return false;
    for (uint8_t c = 0; c < AUTH_CARD_COUNT; c++) {
        bool match = true;
        for (uint8_t i = 0; i < AUTH_UID_LENGTH; i++) {
            if (uid[i] != authorizedCards[c][i]) { match = false; break; }
        }
        if (match) return true;
    }
    return false;
}

void handleRFIDAccess() {
    if (!mfrc522.PICC_IsNewCardPresent()) return;
    if (!mfrc522.PICC_ReadCardSerial())   return;

    Serial.print("RFID UID: ");
    for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) Serial.print("0");
        Serial.print(mfrc522.uid.uidByte[i], HEX);
        if (i < mfrc522.uid.size - 1) Serial.print(":");
    }
    Serial.println();

    if (isAuthorizedUID(mfrc522.uid.uidByte, mfrc522.uid.size)) {
        Serial.println("ACCESS GRANTED");
        authState = AUTH_OK;
    } else {
        Serial.println("ACCESS DENIED");
        authState = AUTH_FAIL;
    }
    authFeedbackStart = millis();
    mfrc522.PICC_HaltA();
    mfrc522.PCD_StopCrypto1();
}

// ==========================================================
// DRAW-FUNKTIONEN
// ==========================================================
void drawMenu() {
    u8g2.drawStr(34, 14, "HAUPTMENU");
    u8g2.drawHLine(0, 17, 126);
    u8g2.drawStr(26, 32, "MSG SENDEN");
    u8g2.drawStr(26, 46, "INBOX");
    u8g2.drawStr(26, 60, "LOGOUT");

    if      (menuSelection == 0) u8g2.drawFrame(20, 22, 90, 14);
    else if (menuSelection == 1) u8g2.drawFrame(20, 36, 90, 14);
    else if (menuSelection == 2) u8g2.drawFrame(20, 50, 90, 14);
}

void drawKeyboard() {
    u8g2.drawStr(0, 10, "IN:");
    u8g2.drawFrame(20, 0, 106, 12);

    // Nur die letzten 16 Zeichen anzeigen (intern bis 64)
    char visible[17];
    if (inputLen <= 16) {
        strncpy(visible, inputBuffer, 16);
        visible[inputLen] = '\0';
    } else {
        strncpy(visible, &inputBuffer[inputLen - 16], 16);
        visible[16] = '\0';
    }
    u8g2.drawStr(23, 10, visible);
    u8g2.drawHLine(0, 15, 126);

    for (uint8_t r = 0; r < 3; r++) {
        for (uint8_t c = 0; c < 10; c++) {
            uint8_t px = c * 12 + 6;
            uint8_t py = r * 13 + 30;
            if (cursorX == c && cursorY == r) {
                u8g2.drawFrame(px - 2, py - 9, 10, 11);
            }
            u8g2.setCursor(px, py);
            u8g2.print(keys[r][c]);
        }
    }
}

void drawInbox() {
    u8g2.drawStr(4, 10, "INBOX");

    char countText[12];
    snprintf(countText, sizeof(countText), "MSG:%u", messageCount);
    u8g2.drawStr(84, 10, countText);
    u8g2.drawHLine(0, 14, 128);

    if (messageCount == 0) {
        u8g2.drawStr(18, 34, "Keine Nachricht");
        u8g2.drawStr(12, 60, "CLICK = ZURUECK");
        return;
    }

    for (uint8_t i = 0; i < 3; i++) {
        uint8_t idx = inboxScroll + i;
        if (idx >= messageCount) break;

        uint8_t y = 26 + i * 12;
        if (idx == inboxSelection) {
            u8g2.drawBox(0, y - 8, 128, 11);
            u8g2.setDrawColor(0);
        }

        char line[4];
        snprintf(line, sizeof(line), "%u:", idx + 1);
        char preview[17];
        strncpy(preview, messages[idx], 16);
        preview[16] = '\0';

        u8g2.drawStr(2, y, line);
        u8g2.drawStr(16, y, preview);

        if (idx == inboxSelection) u8g2.setDrawColor(1);
    }
    u8g2.drawHLine(0, 52, 128);
    u8g2.drawStr(2, 62, "JOY=SCROLL CLICK=EXIT");
}

void drawLockScreen() {
    if (authState == AUTH_IDLE) {
        u8g2.drawStr(10, 22, "Berechtigungs");
        u8g2.drawStr(20, 34, "karte wird");
        u8g2.drawStr(18, 46, "gescannt...");
    } else if (authState == AUTH_OK) {
        u8g2.drawFrame(18, 20, 92, 20);
        u8g2.drawStr(28, 34, "ERFOLGREICH!");
    } else {
        u8g2.drawFrame(28, 20, 72, 20);
        u8g2.drawStr(44, 34, "FALSCH!");
    }
}

void drawSuccessScreen() {
    u8g2.firstPage();
    do {
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.drawFrame(10, 20, 108, 25);
        u8g2.drawStr(25, 36, "SENT SUCCESS!");
    } while (u8g2.nextPage());
}

void drawCurrentScreen() {
    u8g2.setFont(u8g2_font_6x10_tf);
    if (!isUnlocked)               { drawLockScreen(); return; }
    if      (currentMode == MENU)     drawMenu();
    else if (currentMode == KEYBOARD) drawKeyboard();
    else if (currentMode == INBOX)    drawInbox();
}

// ==========================================================
// SETUP
// ==========================================================
void setup() {
    Serial.begin(9600);
    bt.begin(9600);
    pinMode(pinSW, INPUT_PULLUP);

    SPI.begin();
    mfrc522.PCD_Init();

    u8g2.begin();
    u8g2.setFont(u8g2_font_6x10_tf);

    Serial.println("UNO SLAVE gestartet.");
    Serial.println("RFID-Lock aktiv. Bitte Karte scannen.");
}

// ==========================================================
// LOOP
// ==========================================================
void loop() {
    // Eingehende BT-Nachrichten pollen
    readBtIncoming();

    // --- LOCKSCREEN / RFID-AUTH ---
    if (!isUnlocked) {
        if (authState == AUTH_IDLE) {
            handleRFIDAccess();
        } else if (millis() - authFeedbackStart >= authFeedbackMs) {
            if (authState == AUTH_OK) isUnlocked = true;
            authState = AUTH_IDLE;
        }
        u8g2.firstPage();
        do { drawCurrentScreen(); } while (u8g2.nextPage());
        return;
    }

    // --- EINGABEN ---
    bool pressed = updatePressedEvent();
    int  xVal    = analogRead(pinX);
    int  yVal    = analogRead(pinY);
    unsigned long now = millis();

    // --- MENU ---
    if (currentMode == MENU) {
        if (now - lastNavTime > navMs) {
            if      (yVal < 300 && menuSelection > 0) { menuSelection--; lastNavTime = now; }
            else if (yVal > 700 && menuSelection < 2) { menuSelection++; lastNavTime = now; }
        }
        if (pressed) {
            if      (menuSelection == 0) { currentMode = KEYBOARD; cursorX = 0; cursorY = 0; }
            else if (menuSelection == 1) { currentMode = INBOX; }
            else {
                isUnlocked    = false;
                authState     = AUTH_IDLE;
                menuSelection = 0;
                cursorX = cursorY = 0;
                clearInput();
                mfrc522.PICC_HaltA();
                mfrc522.PCD_StopCrypto1();
            }
        }
    }

    // --- KEYBOARD ---
    else if (currentMode == KEYBOARD) {
        if (now - lastNavTime > navMs) {
            if      (xVal < 300 && cursorX > 0) { cursorX--; lastNavTime = now; }
            else if (xVal > 700 && cursorX < 9) { cursorX++; lastNavTime = now; }
            else if (yVal < 300 && cursorY > 0) { cursorY--; lastNavTime = now; }
            else if (yVal > 700 && cursorY < 2) { cursorY++; lastNavTime = now; }
        }
        if (pressed) {
            char sel = keys[cursorY][cursorX];
            if (sel == '<') {
                backspaceChar();
            } else if (sel == '#') {
                if (inputLen > 0) {
                    handleInput(inputBuffer);       // senden
                    saveMessageToInbox(inputBuffer); // auch lokal speichern
                    drawSuccessScreen();
                    delay(900);
                    clearInput();
                    currentMode = MENU;
                }
            } else {
                appendChar(sel);
            }
        }
    }

    // --- INBOX ---
    else if (currentMode == INBOX) {
        if (messageCount > 0 && now - lastNavTime > navMs) {
            if      (yVal < 300 && inboxSelection > 0)                    { inboxSelection--; lastNavTime = now; }
            else if (yVal > 700 && inboxSelection < messageCount - 1)     { inboxSelection++; lastNavTime = now; }

            if (inboxSelection < inboxScroll)           inboxScroll = inboxSelection;
            if (inboxSelection >= inboxScroll + 3)      inboxScroll = inboxSelection - 2;
        }
        if (pressed) currentMode = MENU;
    }

    // --- DISPLAY ---
    u8g2.firstPage();
    do { drawCurrentScreen(); } while (u8g2.nextPage());
}
