#include <SoftwareSerial.h>

SoftwareSerial bt(2, 3); // RX=2, TX=3

String eingehend = "";

void setup() {
  Serial.begin(9600);
  bt.begin(9600);
  Serial.println("=== BT Messenger bereit ===");
}

void loop() {
  // Empfangen vom iPhone
  while (bt.available()) {
    char c = (char)bt.read();
    if (c == '\n') {
      if (eingehend.length() > 0) {
        Serial.print("[iPhone] ");
        Serial.println(eingehend);
        
        // Antwort zurückschicken
        bt.print("Arduino: " + eingehend + "\n");
      }
      eingehend = "";
    } else if (c != '\r') {
      eingehend += c;
    }
  }

  // Senden vom Serial Monitor ans iPhone
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      // wird über BT gesendet
    } else {
      bt.write(c);
    }
  }
}
