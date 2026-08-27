#include "BleServer.h"

#include <NimBLEDevice.h>
#include "storage/Storage.h"

namespace
{
    constexpr char SERVICE_UUID[] = "4fa8691a-1360-4c27-ba5c-057245417c92";
    constexpr char DATA_UUID[] = "4fa8691b-1360-4c27-ba5c-057245417c92";
    constexpr char COMMAND_UUID[] = "4fa8691c-1360-4c27-ba5c-057245417c92";
    NimBLECharacteristic *dataCharacteristic = nullptr;
    StorageController *activeStorage = nullptr;
    bool clientConnected = false;

    // Pushes one chunk to the frontend by notifying dataCharacteristic.
    void sendChunk(const String &chunk)
    {
        dataCharacteristic->setValue(chunk.c_str());
        const bool ok = dataCharacteristic->notify();
        Serial.printf("sendChunk: %u bytes, notify %s\n", chunk.length(), ok ? "ok" : "FAILED");
        delay(20);
    }

    class CommandCallbacks : public NimBLECharacteristicCallbacks
    {
    public:
        void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override
        {
            const String command = characteristic->getValue().c_str();
            Serial.printf("BLE command received: %s\n", command.c_str());
            if (command.startsWith("CLIENT_NAME:"))
            {
                Serial.printf("BLE client name: %s\n", command.substring(12).c_str());
                return;
            }
            if (command == "LIST")
            {
                String files;
                if (!activeStorage || !activeStorage->listAllFiles(files))
                {
                    Serial.println("listAllFiles() failed or storage unavailable.");
                }
                Serial.printf("listAllFiles() returned %u bytes:\n%s\n", files.length(), files.c_str());
                sendChunk("BEGIN LIST\n" + files + "END LIST\n");
                Serial.println("BLE list response complete");
            }
            else if (command.startsWith("GET:"))
            {
                const String path = command.substring(4);
                Serial.printf("BLE command received: GET %s\n", path.c_str());
                sendChunk("BEGIN CSV\n");
                // Only the most recent readings are sent: BLE bandwidth and graph readability don't scale to entire logs.
                const bool sent = activeStorage && activeStorage->streamRecentLines(path, 300, sendChunk);
                if (!sent)
                {
                    sendChunk("ERROR FILE\n");
                }
                sendChunk("END CSV\n");
                Serial.println("BLE CSV response complete");
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
    NimBLEDevice::setMTU(247);
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
