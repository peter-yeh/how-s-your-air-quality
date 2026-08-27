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
    volatile bool transferInProgress = false;

    void sendChunk(const String &chunk, void *)
    {
        dataCharacteristic->setValue(chunk.c_str());
        dataCharacteristic->notify();
        delay(100);
    }

    class CommandCallbacks : public NimBLECharacteristicCallbacks
    {
    public:
        void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override
        {
            const String command = characteristic->getValue().c_str();
            if (command.startsWith("CLIENT_NAME:"))
            {
                Serial.printf("BLE client name: %s\n", command.substring(12).c_str());
                return;
            }
            transferInProgress = true;
            if (command == "LIST")
            {
                String files;
                Serial.println("BLE command received: LIST");
                sendChunk("BEGIN LIST\n", nullptr);
                if (activeStorage && activeStorage->listCsvFiles(files))
                {
                    Serial.printf("CSV list size: %u bytes\n", files.length());
                    for (size_t start = 0; start < files.length(); start += 180)
                    {
                        sendChunk(files.substring(start, start + 180), nullptr);
                    }
                }
                sendChunk("END LIST\n", nullptr);
                Serial.println("BLE list response complete");
            }
            else if (command.startsWith("GET:"))
            {
                Serial.printf("BLE command received: GET %s\n", command.substring(4).c_str());
                sendChunk("BEGIN CSV\n", nullptr);
                const bool sent = activeStorage && activeStorage->streamFile(command.substring(4), sendChunk, nullptr);
                if (!sent)
                {
                    sendChunk("ERROR FILE\n", nullptr);
                }
                sendChunk("END CSV\n", nullptr);
            }
            transferInProgress = false;
        }
    };

    class ServerCallbacks : public NimBLEServerCallbacks
    {
    public:
        void onConnect(NimBLEServer *, NimBLEConnInfo &connInfo) override
        {
            clientConnected = true;
            Serial.printf("BLE client connected, peer address: %s\n", connInfo.getAddress().toString().c_str());
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
    NimBLEServer *server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());
    NimBLEService *service = server->createService(SERVICE_UUID);
    dataCharacteristic = service->createCharacteristic(DATA_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    NimBLECharacteristic *commandCharacteristic = service->createCharacteristic(COMMAND_UUID, NIMBLE_PROPERTY::WRITE);
    commandCharacteristic->setCallbacks(new CommandCallbacks());
    dataCharacteristic->setValue("Air Quality Monitor ready");
    service->start();

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
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
