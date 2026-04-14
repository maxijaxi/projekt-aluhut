#include <SoftwareSerial.h>
SoftwareSerial bt(2, 3);

String eingehend = "";
String ausgehend = "";

void setup() {
  Serial.begin(9600);
  bt.begin(9600);
  delay(1000);
  Serial.println("=== BT Messenger bereit ===");
}

void loop() {
  while (bt.available()) {
    char c = (char)bt.read();
    if (c == '\n') {
      if (eingehend.length() > 0 &&
          !eingehend.startsWith("OK") &&
          !eingehend.startsWith("+")) {
        Serial.print("[Empfangen] ");
        Serial.println(eingehend);
      }
      eingehend = "";
    } else if (c != '\r') {
      eingehend += c;
    }
  }

  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      bt.print(ausgehend + "\n");
      Serial.print("[Gesendet]  ");
      Serial.println(ausgehend);
      ausgehend = "";
    } else if (c != '\r') {
      ausgehend += c;
    }
  }
}
