#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <string.h>
#include <SPI.h>
#include <MFRC522.h>

// ============================================================
// SECTION 1: DISPLAY
// _1_ = Page Buffer -> spart viel RAM auf Uno/Nano
// ============================================================
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ============================================================
// SECTION 2: PIN-DEFINITIONEN
// ============================================================
const uint8_t pinX  = A0;
const uint8_t pinY  = A1;
const uint8_t pinSW = 2;

// RFID RC522
const uint8_t SS_PIN  = 10;
const uint8_t RST_PIN = 9;
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ============================================================
// SECTION 3: RFID WHITELIST
// >>> HIER DEINE ERLAUBTEN RFID UIDS EINTRAGEN <<<
// ============================================================
const uint8_t AUTH_UID_LENGTH = 4;
const uint8_t authorizedCards[][AUTH_UID_LENGTH] = {
  {0x2D, 0x40, 0x98, 0x38}
};
const uint8_t AUTH_CARD_COUNT = sizeof(authorizedCards) / sizeof(authorizedCards[0]);

// ============================================================
// SECTION 4: LOCKSCREEN STATUS
// ============================================================
bool isUnlocked = false;

enum AuthFeedback { AUTH_IDLE, AUTH_OK, AUTH_FAIL };
AuthFeedback authState = AUTH_IDLE;

unsigned long authFeedbackStart = 0;
const unsigned long authFeedbackDuration = 2000;

// ============================================================
// SECTION 5: APP-MODI
// ============================================================
enum Mode { MENU, KEYBOARD, INBOX };
Mode currentMode = MENU;

// ============================================================
// SECTION 6: MENÜ / CURSOR
// 0 = MSG SENDEN
// 1 = INBOX
// 2 = LOGOUT
// ============================================================
uint8_t menuSelection = 0;
uint8_t cursorX = 0;
uint8_t cursorY = 0;

// ============================================================
// SECTION 7: TASTATUR-LAYOUT
// '<' = Backspace
// '#' = Senden
// ============================================================
const char keys[3][10] = {
  {'A','B','C','D','E','F','G','H','I','J'},
  {'K','L','M','N','O','P','Q','R','S','T'},
  {'U','V','W','X','Y','Z','1','2','<','#'}
};

// ============================================================
// SECTION 8: INPUT-BUFFER
// USER-VORGABE:
// MAX 64 Zeichen
// ============================================================
const int MAX_MSG_SIZE = 64;
char inputBuffer[MAX_MSG_SIZE + 1] = "";
int inputLen = 0;

// ============================================================
// SECTION 9: INBOX-SPEICHER
// Wegen RAM auf Uno/Nano lieber wenige Nachrichten speichern
// ============================================================
const uint8_t MAX_MESSAGES = 4;
char messages[MAX_MESSAGES][MAX_MSG_SIZE + 1];
uint8_t messageCount = 0;
uint8_t inboxSelection = 0;
uint8_t inboxScroll = 0;

// ============================================================
// SECTION 10: BUTTON-DEBOUNCE
// ============================================================
bool lastReading = HIGH;
bool stableButton = HIGH;
bool lastStableButton = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceMs = 20;

// ============================================================
// SECTION 11: NAVIGATION TIMING
// ============================================================
unsigned long lastNavTime = 0;
const unsigned long navMs = 180;

// ============================================================
// SECTION 12: DOCKING-FUNKTION FÜR INPUT-WEITERGABE
// USER-VORGABE:
// handleInput(_input);
// ============================================================
void handleInput(char* _input) {
  // Hier kannst du später die Nachricht weiterverarbeiten:
  // z.B. per Serial senden, Funkmodul, ESP, LoRa, etc.
}

// ============================================================
// SECTION 13: BUTTON-KLICK ERKENNEN
// ============================================================
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

// ============================================================
// SECTION 14: INPUT-FUNKTIONEN
// ============================================================
void appendChar(char c) {
  if (inputLen < MAX_MSG_SIZE) {
    inputBuffer[inputLen++] = c;
    inputBuffer[inputLen] = '\0';
  }
}

void backspaceChar() {
  if (inputLen > 0) {
    inputLen--;
    inputBuffer[inputLen] = '\0';
  }
}

void clearInput() {
  inputLen = 0;
  inputBuffer[0] = '\0';
}

// ============================================================
// SECTION 15: NACHRICHT IN INBOX SPEICHERN
// Wenn voll, wird die älteste Nachricht entfernt
// ============================================================
void saveMessageToInbox() {
  if (inputLen == 0) return;

  if (messageCount < MAX_MESSAGES) {
    strncpy(messages[messageCount], inputBuffer, MAX_MSG_SIZE);
    messages[messageCount][MAX_MSG_SIZE] = '\0';
    messageCount++;
  } else {
    for (uint8_t i = 0; i < MAX_MESSAGES - 1; i++) {
      strncpy(messages[i], messages[i + 1], MAX_MSG_SIZE + 1);
    }
    strncpy(messages[MAX_MESSAGES - 1], inputBuffer, MAX_MSG_SIZE);
    messages[MAX_MESSAGES - 1][MAX_MSG_SIZE] = '\0';
  }

  if (messageCount > 0) inboxSelection = messageCount - 1;
  else inboxSelection = 0;

  inboxScroll = (inboxSelection >= 2) ? (inboxSelection - 2) : 0;
}

// ============================================================
// SECTION 16: RFID HILFSFUNKTIONEN
// ============================================================
void printUIDToSerial(byte *buffer, byte bufferSize) {
  Serial.print("RFID UID: ");
  for (byte i = 0; i < bufferSize; i++) {
    if (buffer[i] < 0x10) Serial.print("0");
    Serial.print(buffer[i], HEX);
    if (i < bufferSize - 1) Serial.print(":");
  }
  Serial.println();
}

bool isAuthorizedUID(byte *uid, byte uidSize) {
  if (uidSize != AUTH_UID_LENGTH) return false;

  for (uint8_t card = 0; card < AUTH_CARD_COUNT; card++) {
    bool match = true;

    for (uint8_t i = 0; i < AUTH_UID_LENGTH; i++) {
      if (uid[i] != authorizedCards[card][i]) {
        match = false;
        break;
      }
    }

    if (match) return true;
  }

  return false;
}

void handleRFIDAccess() {
  if (!mfrc522.PICC_IsNewCardPresent()) return;
  if (!mfrc522.PICC_ReadCardSerial()) return;

  printUIDToSerial(mfrc522.uid.uidByte, mfrc522.uid.size);

  if (isAuthorizedUID(mfrc522.uid.uidByte, mfrc522.uid.size)) {
    Serial.println("ACCESS GRANTED");
    authState = AUTH_OK;
    authFeedbackStart = millis();
  } else {
    Serial.println("ACCESS DENIED");
    authState = AUTH_FAIL;
    authFeedbackStart = millis();
  }

  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}

// ============================================================
// SECTION 17: MENÜ ZEICHNEN
// ============================================================
void drawMenu() {
  u8g2.drawStr(34, 14, "HAUPTMENU");
  u8g2.drawHLine(0, 17, 126);

  u8g2.drawStr(26, 32, "MSG SENDEN");
  u8g2.drawStr(26, 46, "INBOX");
  u8g2.drawStr(26, 60, "LOGOUT");

  if (menuSelection == 0) u8g2.drawFrame(20, 22, 90, 14);
  else if (menuSelection == 1) u8g2.drawFrame(20, 36, 90, 14);
  else if (menuSelection == 2) u8g2.drawFrame(20, 50, 90, 14);
}

// ============================================================
// SECTION 18: TASTATUR ZEICHNEN
// Für das Display werden nur die letzten 16 Zeichen oben gezeigt,
// intern bleiben aber bis zu 64 Zeichen gespeichert.
// ============================================================
void drawKeyboard() {
  u8g2.drawStr(0, 10, "IN:");
  u8g2.drawFrame(20, 0, 106, 12);

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
      uint8_t posX = c * 12 + 6;
      uint8_t posY = r * 13 + 30;

      if (cursorX == c && cursorY == r) {
        u8g2.drawFrame(posX - 2, posY - 9, 10, 11);
      }

      u8g2.setCursor(posX, posY);
      if (keys[r][c] == '#') u8g2.print("#");
      else u8g2.print(keys[r][c]);
    }
  }
}

// ============================================================
// SECTION 19: INBOX ZEICHNEN
// Kompakte Anzeige auf kleinem OLED
// ============================================================
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
    uint8_t msgIndex = inboxScroll + i;
    if (msgIndex >= messageCount) break;

    uint8_t y = 26 + (i * 12);

    if (msgIndex == inboxSelection) {
      u8g2.drawBox(0, y - 8, 128, 11);
      u8g2.setDrawColor(0);
    }

    char line[22];
    snprintf(line, sizeof(line), "%u:", msgIndex + 1);

    char preview[17];
    strncpy(preview, messages[msgIndex], 16);
    preview[16] = '\0';

    u8g2.drawStr(2, y, line);
    u8g2.drawStr(16, y, preview);

    if (msgIndex == inboxSelection) {
      u8g2.setDrawColor(1);
    }
  }

  u8g2.drawHLine(0, 52, 128);
  u8g2.drawStr(2, 62, "JOY=SCROLL CLICK=EXIT");
}

// ============================================================
// SECTION 20: LOCKSCREEN ZEICHNEN
// ============================================================
void drawLockScreen() {
  if (authState == AUTH_IDLE) {
    u8g2.drawStr(10, 22, "Berechtigungs");
    u8g2.drawStr(20, 34, "karte wird");
    u8g2.drawStr(18, 46, "gescannt...");
  } 
  else if (authState == AUTH_OK) {
    u8g2.drawFrame(18, 20, 92, 20);
    u8g2.drawStr(28, 34, "ERFOLGREICH!");
  } 
  else if (authState == AUTH_FAIL) {
    u8g2.drawFrame(28, 20, 72, 20);
    u8g2.drawStr(44, 34, "FALSCH!");
  }
}

// ============================================================
// SECTION 21: SUCCESS SCREEN
// ============================================================
void drawSuccessScreen() {
  u8g2.firstPage();
  do {
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawFrame(10, 20, 108, 25);
    u8g2.drawStr(25, 36, "SENT SUCCESS!");
  } while (u8g2.nextPage());
}

// ============================================================
// SECTION 22: AKTIVEN SCREEN ZEICHNEN
// ============================================================
void drawCurrentScreen() {
  u8g2.setFont(u8g2_font_6x10_tf);

  if (!isUnlocked) {
    drawLockScreen();
    return;
  }

  if (currentMode == MENU) drawMenu();
  else if (currentMode == KEYBOARD) drawKeyboard();
  else if (currentMode == INBOX) drawInbox();
}

// ============================================================
// SECTION 23: SETUP
// ============================================================
void setup() {
  Serial.begin(9600);
  pinMode(pinSW, INPUT_PULLUP);

  SPI.begin();
  mfrc522.PCD_Init();

  u8g2.begin();
  u8g2.setFont(u8g2_font_6x10_tf);

  Serial.println("System gestartet.");
  Serial.println("RFID Lock aktiv. Bitte Karte scannen.");
}

// ============================================================
// SECTION 24: LOOP
// ============================================================
void loop() {
  // ---------------- LOCKSCREEN / RFID AUTH ----------------
  if (!isUnlocked) {
    if (authState == AUTH_IDLE) {
      handleRFIDAccess();
    } else {
      if (millis() - authFeedbackStart >= authFeedbackDuration) {
        if (authState == AUTH_OK) {
          isUnlocked = true;
          authState = AUTH_IDLE;
        } else {
          authState = AUTH_IDLE;
        }
      }
    }

    u8g2.firstPage();
    do {
      drawCurrentScreen();
    } while (u8g2.nextPage());

    return;
  }

  // ---------------- NORMALE EINGABEN ----------------
  bool pressed = updatePressedEvent();
  int xVal = analogRead(pinX);
  int yVal = analogRead(pinY);
  unsigned long now = millis();

  // ---------------- MENU ----------------
  if (currentMode == MENU) {
    if (now - lastNavTime > navMs) {
      if (yVal < 300 && menuSelection > 0) {
        menuSelection--;
        lastNavTime = now;
      } 
      else if (yVal > 700 && menuSelection < 2) {
        menuSelection++;
        lastNavTime = now;
      }
    }

    if (pressed) {
      if (menuSelection == 0) {
        currentMode = KEYBOARD;
        cursorX = 0;
        cursorY = 0;
      } 
      else if (menuSelection == 1) {
        currentMode = INBOX;
      } 
      else if (menuSelection == 2) {
        isUnlocked = false;
        authState = AUTH_IDLE;
        menuSelection = 0;
        cursorX = 0;
        cursorY = 0;
        clearInput();

        mfrc522.PICC_HaltA();
        mfrc522.PCD_StopCrypto1();
      }
    }
  }

  // ---------------- KEYBOARD ----------------
  else if (currentMode == KEYBOARD) {
    if (now - lastNavTime > navMs) {
      if (xVal < 300 && cursorX > 0) {
        cursorX--;
        lastNavTime = now;
      } 
      else if (xVal > 700 && cursorX < 9) {
        cursorX++;
        lastNavTime = now;
      } 
      else if (yVal < 300 && cursorY > 0) {
        cursorY--;
        lastNavTime = now;
      } 
      else if (yVal > 700 && cursorY < 2) {
        cursorY++;
        lastNavTime = now;
      }
    }

    if (pressed) {
      char selected = keys[cursorY][cursorX];

      if (selected == '<') {
        backspaceChar();
      } 
      else if (selected == '#') {
        if (inputLen > 0) {
          handleInput(inputBuffer);
          saveMessageToInbox();
          drawSuccessScreen();
          delay(900);
          clearInput();
          currentMode = MENU;
        }
      } 
      else {
        appendChar(selected);
      }
    }
  }

  // ---------------- INBOX ----------------
  else if (currentMode == INBOX) {
    if (messageCount > 0 && now - lastNavTime > navMs) {
      if (yVal < 300 && inboxSelection > 0) {
        inboxSelection--;
        lastNavTime = now;
      } 
      else if (yVal > 700 && inboxSelection < messageCount - 1) {
        inboxSelection++;
        lastNavTime = now;
      }

      if (inboxSelection < inboxScroll) {
        inboxScroll = inboxSelection;
      }
      if (inboxSelection >= inboxScroll + 3) {
        inboxScroll = inboxSelection - 2;
      }
    }

    if (pressed) {
      currentMode = MENU;
    }
  }

  // ---------------- DISPLAY ----------------
  u8g2.firstPage();
  do {
    drawCurrentScreen();
  } while (u8g2.nextPage());
}
