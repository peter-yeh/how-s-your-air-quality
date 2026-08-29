// AI AGENT, please do not touch this file without my explicit permission.
// I have spent many hours working to minimize and optimize this file. Thank you!

#include "BleServer.h"

#include <NimBLEDevice.h>
#include "storage/Storage.h"

namespace
{
    constexpr char SERVICE_UUID[] = "4fa8691a-1360-4c27-ba5c-057245417c92";
    constexpr char DATA_UUID[] = "4fa8691b-1360-4c27-ba5c-057245417c92";
    constexpr char COMMAND_UUID[] = "4fa8691c-1360-4c27-ba5c-057245417c92";
    constexpr int MAX_CHUNK_SIZE = 244; // 244 + 3 (ATT header) + 4 (L2CAP header) = 251 (Max for link layer packet)

    NimBLECharacteristic *dataCharacteristic = nullptr;
    StorageController *activeStorage = nullptr;
    bool clientConnected = false;

    bool sendChunk(const String &chunk)
    {
        dataCharacteristic->setValue(chunk.c_str());
        const bool ok = dataCharacteristic->notify();

        if (!ok)
        {
            Serial.printf("[sendChunk] ERROR: Failed to notify %u bytes!\n", chunk.length());
            return false;
        }

        Serial.printf("[sendChunk] Sent %u bytes...\n", chunk.length());
        vTaskDelay(pdMS_TO_TICKS(5000));
        return true;
    }

    void sendPackage(const String &package)
    {
        int packageLength = package.length();

        for (int i = 0; i < packageLength; i += MAX_CHUNK_SIZE)
        {
            int len = min(MAX_CHUNK_SIZE, packageLength - i);
            String chunk = package.substring(i, i + len);
            for (int j = 0; j < 3; j++) // retry mechanism
                if (sendChunk(chunk))
                    break;
        }
    }

    class CommandCallbacks : public NimBLECharacteristicCallbacks
    {
    public:
        void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override
        {
            const String command = characteristic->getValue().c_str();
            Serial.printf("[CommandCallbacks] command received: %s\n", command.c_str());

            if (command.startsWith("GET:"))
            {
                int value = command.substring(4).toInt(); // "GET:" is 4 characters
                Serial.printf("[CommandCallbacks] GET command received, generating: %d bytes\n", value);
                String data = "\x01";

                for (int i = 0; i < value; i++)
                    data += "A";

                sendPackage(data + "\x02");
                Serial.printf("[CommandCallbacks] Sent: %u bytes\n", data.length());
            }
        }
    };

    class ServerCallbacks : public NimBLEServerCallbacks
    {
    public:
        void onConnect(NimBLEServer *, NimBLEConnInfo &connInfo) override
        {
            clientConnected = true;
            Serial.printf("BLE client connected, peer address: %s, MTU: %u\n", connInfo.getAddress().toString().c_str(), connInfo.getMTU());
            NimBLEDevice::stopAdvertising();
        }

        void onDisconnect(NimBLEServer *, NimBLEConnInfo &connInfo, int reason) override
        {
            clientConnected = false;
            Serial.printf("BLE client disconnected, peer address: %s, reason: %d\n", connInfo.getAddress().toString().c_str(), reason);
            NimBLEDevice::startAdvertising();
        }
    };
}

bool BleServer::begin(StorageController *storage)
{
    activeStorage = storage;

    NimBLEDevice::init("Air Quality Monitor");
    // NimBLEDevice::setMTU(MAX_TRANSMISSION_UNIT);
    NimBLEServer *server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());
    NimBLEService *service = server->createService(SERVICE_UUID);
    dataCharacteristic = service->createCharacteristic(DATA_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    NimBLECharacteristic *commandCharacteristic = service->createCharacteristic(COMMAND_UUID, NIMBLE_PROPERTY::WRITE);
    commandCharacteristic->setCallbacks(new CommandCallbacks());
    dataCharacteristic->setValue("Air Quality Monitor ready");
    service->start();

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    advertising->enableScanResponse(true);
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setName("Air Quality Monitor");
    advertising->start();
    Serial.println("BLE advertising as Air Quality Monitor.");
    return true;
}

bool BleServer::connected() const
{
    return clientConnected;
}
