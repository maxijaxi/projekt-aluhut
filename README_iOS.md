# AluhutChat – iOS Swift App

Eine iOS-App zum Chatten mit dem Arduino über das **HM-10 BLE-Modul**.  
Die App verwendet die gleiche Verschlüsselung, Prüfsumme und das gleiche Paketformat wie der Arduino-Code in diesem Repository.

---

## Protokoll (Arduino ↔ iOS)

| Schicht | Details |
|---------|---------|
| **Transport** | BLE, HM-10 Service `FFE0`, Characteristic `FFE1` (notify + write without response) |
| **Verschlüsselung** | XOR mit Key `0xAA` (symmetrisch, entspricht `util/encryption.cpp`) |
| **Prüfsumme** | Summe aller Bytes des Klartexts mod 256 (entspricht `util/checkSum.cpp`) |
| **Paketformat** | `{XOR-verschlüsselte Daten}\x03{Prüfsumme dezimal}\n` (entspricht `util/readyForSend.cpp`) |

Der iOS-Stack:
1. **Senden:** Klartext → Prüfsumme berechnen → XOR-verschlüsseln → `ETX + Prüfsumme + \n` anhängen → in 20-Byte-Chunks via BLE schreiben
2. **Empfangen:** Chunks puffern bis `\n` → ETX-Trenner finden → XOR-entschlüsseln → Prüfsumme verifizieren → Text anzeigen

---

## Dateistruktur

```
AluhutChat/
├── AluhutChatApp.swift       # App-Entry-Point (@main)
├── Models/
│   └── Message.swift         # Nachrichten-Datenmodell
├── Utilities/
│   └── Crypto.swift          # XOR, Prüfsumme, Paket-Builder/-Parser
├── Bluetooth/
│   └── BLEManager.swift      # CoreBluetooth – Scan, Connect, Send, Receive
└── Views/
    ├── ContentView.swift      # Root-View (wechselt zwischen Geräteliste und Chat)
    ├── DeviceListView.swift   # BLE-Scanner & Geräteauswahl
    └── ChatView.swift         # Chat-UI mit Nachrichtenblasen
```

---

## Xcode-Projekt erstellen

### Voraussetzungen

- macOS 13+ mit Xcode 15+
- iOS-Gerät oder Simulator mit iOS 16+
- Für echten Bluetooth-Test: iPhone (Simulatoren unterstützen kein BLE zu Hardware)

### Schritt-für-Schritt

1. **Xcode öffnen → File → New → Project**
   - Template: **iOS → App**
   - Product Name: `AluhutChat`
   - Interface: `SwiftUI`
   - Language: `Swift`
   - Bundle Identifier: z. B. `com.deinname.aluhutchat`
   - Zielordner wählen (kann innerhalb dieses Repos sein)

2. **Dateien ersetzen / hinzufügen**
   - Die von Xcode erstellte `ContentView.swift` und `<Projektname>App.swift` **löschen**.
   - Alle Swift-Dateien aus `AluhutChat/` per Drag & Drop in den Xcode-Navigator ziehen  
     (Häkchen bei „Copy items if needed" setzen, Gruppe `AluhutChat` wählen).

3. **Info.plist – Bluetooth-Berechtigungen**  
   Folgende Schlüssel in `Info.plist` eintragen (Xcode: Projekt → Target → Info → Custom iOS Target Properties):

   | Key | Wert (Beispiel) |
   |-----|-----------------|
   | `NSBluetoothAlwaysUsageDescription` | `Die App benötigt Bluetooth, um mit dem Arduino zu kommunizieren.` |
   | `NSBluetoothPeripheralUsageDescription` | `Die App benötigt Bluetooth, um mit dem HM-10 Modul zu chatten.` |

4. **Deployment Target** auf **iOS 16.0** oder höher setzen  
   (Projekt → Target → General → Minimum Deployments).

5. **Bauen & Starten** (⌘R)

---

## Arduino-Seite anpassen

Damit der Arduino Nachrichten über das HM-10 Modul senden und empfangen kann, muss die `handleInput`-Funktion in `raphi.cpp` implementiert werden.  
Das HM-10 ist per Software-Serial oder Hardware-Serial angeschlossen und leitet BLE-Daten 1:1 als serielle Bytes weiter.

Minimale Implementierung (HM-10 an Hardware Serial 1 auf einem Mega/Due):

```cpp
#include "util/encryption.h"
#include "util/checkSum.h"

// readyForSend.cpp Funktion
void makeReadyForSend(char* _data, char* _output);

void handleInput(char* _input) {
    // Paket bauen und über HM-10 senden
    char encrypted[MAX_PACKET_SIZE];
    char packet[MAX_PACKET_SIZE + 10];

    xorEncrypt(_input, encrypted);          // XOR verschlüsseln
    makeReadyForSend(encrypted, packet);    // Prüfsumme anhängen
    Serial1.print(packet);                  // An HM-10 senden
}
```

Für den Empfang (Nachrichten von der iOS-App):

```cpp
// In loop() aufrufen:
void checkBLEReceive() {
    static char recvBuf[MAX_PACKET_SIZE + 10];
    static size_t recvLen = 0;

    while (Serial1.available()) {
        char c = Serial1.read();
        if (recvLen < sizeof(recvBuf) - 1) {
            recvBuf[recvLen++] = c;
        }
        if (c == '\n') {
            recvBuf[recvLen] = '\0';
            // Paket parsen: Trenne an ETX (0x03)
            char* etxPtr = strchr(recvBuf, 0x03);
            if (etxPtr != nullptr) {
                *etxPtr = '\0';
                // recvBuf enthält jetzt die verschlüsselten Daten
                char decrypted[MAX_MSG_SIZE + 1];
                xorEncrypt(recvBuf, decrypted); // XOR = symmetrisch
                // Nachricht in Inbox speichern:
                strncpy(inputBuffer, decrypted, MAX_MSG_SIZE);
                inputLen = strlen(decrypted);
                saveMessageToInbox();
            }
            recvLen = 0;
        }
    }
}
```

---

## Bekannte Einschränkungen

- HM-10 überträgt max. **20 Byte pro BLE-Paket** – längere Nachrichten werden automatisch in Chunks aufgeteilt.
- Die maximale Nachrichtenlänge ist durch den Arduino auf **64 Zeichen** begrenzt (`MAX_MSG_SIZE` in `config.h`).
- Der iOS-Simulator unterstützt **kein echtes BLE** zu externer Hardware. Für Tests bitte ein echtes iPhone verwenden.
