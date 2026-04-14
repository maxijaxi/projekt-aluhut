#include <NimBLEDevice.h>

#define SERVICE_UUID "0000FFE0-0000-1000-8000-00805F9B34FB"
#define CHARACTERISTIC_UUID "0000FFE1-0000-1000-8000-00805F9B34FB"
#define SLAVE_ADDRESS "78:04:73:c5:07:81"

NimBLERemoteCharacteristic *pChar = nullptr;
bool connected = false;

void notifyCallback(NimBLERemoteCharacteristic *c, uint8_t *data, size_t len, bool isNotify)
{
    for (int i = 0; i < len; i++)
        Serial.write(data[i]);
}

void doConnect()
{
    Serial.println("Verbinde zu HM-10...");
    NimBLEClient *client = NimBLEDevice::createClient();

    if (!client->connect(NimBLEAddress(SLAVE_ADDRESS, BLE_ADDR_PUBLIC)))
    {
        Serial.println("Verbindung fehlgeschlagen, retry...");
        return;
    }

    NimBLERemoteService *service = client->getService(SERVICE_UUID);
    if (!service)
    {
        Serial.println("Service nicht gefunden");
        client->disconnect();
        return;
    }

    pChar = service->getCharacteristic(CHARACTERISTIC_UUID);
    if (!pChar)
    {
        Serial.println("Characteristic nicht gefunden");
        client->disconnect();
        return;
    }

    pChar->subscribe(true, notifyCallback);
    connected = true;
    Serial.println("=== Verbunden! ===");
}

void setup()
{
    Serial.begin(115200);
    NimBLEDevice::init("ESP32");
    doConnect();
}

void loop()
{
    if (!connected)
    {
        delay(3000);
        doConnect();
        return;
    }

    if (Serial.available())
    {
        String msg = Serial.readStringUntil('\n');
        pChar->writeValue(msg + "\n");
    }
}