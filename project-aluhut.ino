#include <SoftwareSerial.h>

SoftwareSerial bt(2, 3);  // RX=Pin2, TX=Pin3

String eingehend = "";
String ausgehend = "";

void setup() {
  Serial.begin(9600);
  bt.begin(9600);
  Serial.println("=== Aluhut Ready ===");
  Serial.println("Tippe eine Nachricht und druecke Enter:");
}

void loop() {
  // Eingehende Nachricht via BT
  while (bt.available()) {
    char c = (char)bt.read();
    if (c == '\n') {
      Serial.print("[Empfangen] ");
      Serial.println(eingehend);
      eingehend = "";
    } else {
      eingehend += c;
    }
  }

  // Ausgehende Nachricht vom Serial Monitor
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      bt.println(ausgehend);
      Serial.print("[Gesendet]  ");
      Serial.println(ausgehend);
      ausgehend = "";
    } else {
      ausgehend += c;
    }
  }
}
