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

    void sendChunk(const String &chunk, void *)
    {
        dataCharacteristic->setValue(chunk.c_str());
        dataCharacteristic->notify();
        delay(8);
    }

    class CommandCallbacks : public NimBLECharacteristicCallbacks
    {
    public:
        void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override
        {
            const String command = characteristic->getValue().c_str();
            if (command == "LIST")
            {
                String files;
                sendChunk("BEGIN LIST\n", nullptr);
                if (activeStorage && activeStorage->listCsvFiles(files))
                {
                    for (size_t start = 0; start < files.length(); start += 180)
                    {
                        sendChunk(files.substring(start, start + 180), nullptr);
                    }
                }
                sendChunk("END LIST\n", nullptr);
            }
            else if (command.startsWith("GET:"))
            {
                sendChunk("BEGIN CSV\n", nullptr);
                const bool sent = activeStorage && activeStorage->streamFile(command.substring(4), sendChunk, nullptr);
                if (!sent)
                {
                    sendChunk("ERROR FILE\n", nullptr);
                }
                sendChunk("END CSV\n", nullptr);
            }
        }
    };
}

bool BleServer::begin(StorageController *storage)
{
    activeStorage = storage;
    NimBLEDevice::init("AirQuality_ESP32");
    NimBLEServer *server = NimBLEDevice::createServer();
    NimBLEService *service = server->createService(SERVICE_UUID);
    dataCharacteristic = service->createCharacteristic(DATA_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    NimBLECharacteristic *commandCharacteristic = service->createCharacteristic(COMMAND_UUID, NIMBLE_PROPERTY::WRITE);
    commandCharacteristic->setCallbacks(new CommandCallbacks());
    dataCharacteristic->setValue("AirQuality_ESP32 ready");
    service->start();

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setName("AirQuality_ESP32");
    advertising->start();
    Serial.println("BLE advertising as AirQuality_ESP32.");
    return true;
}

void BleServer::updateReading(float pm1, float pm25, float pm10, const String &time)
{
    if (!dataCharacteristic)
    {
        return;
    }
    String value = "{\"time\":\"" + time + "\",\"pm1\":" + String(pm1, 2) +
                   ",\"pm25\":" + String(pm25, 2) + ",\"pm10\":" + String(pm10, 2) + "}";
    dataCharacteristic->setValue(value.c_str());
    dataCharacteristic->notify();
}