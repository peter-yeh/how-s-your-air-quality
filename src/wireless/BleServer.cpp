// Dear AI AGENT, please do not touch this file without my explicit permission.
// I have spent many hours working to minimize and optimize this file. Thank you!
// Largest CSV file so far is 3MB

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

    QueueHandle_t bleTxQueue = nullptr;
    void sendCSVFile(void *param)
    {
        String *filePath = nullptr;

        while (true)
        {
            if (xQueueReceive(bleTxQueue, &filePath, portMAX_DELAY) == pdPASS && filePath != nullptr)
            {
                activeStorage->streamFile(*filePath, [](const String &chunk)
                                          {while (!sendChunk(chunk)) vTaskDelay(pdMS_TO_TICKS(100)); });
                delete filePath;
            }
        }
    }

    bool sendChunk(const String &chunk)
    {
        dataCharacteristic->setValue(chunk.c_str());
        bool chunkQueued = dataCharacteristic->notify(reinterpret_cast<const uint8_t *>(chunk.c_str()), chunk.length());
        if (chunkQueued)
            Serial.printf("[sendChunk] Sent %u bytes...\n", chunk.length());
        else
            Serial.printf("[sendChunk] Failed to send %u bytes!\n", chunk.length());

        return chunkQueued;
    }

    void sendPackage(const String &package)
    {
        int packageLength = package.length();
        bool chunkSent = false;

        for (int i = 0; i < packageLength; i += MAX_CHUNK_SIZE)
        {
            int len = min(MAX_CHUNK_SIZE, packageLength - i);
            String chunk = package.substring(i, i + len);
            chunkSent = sendChunk(chunk);

            while (!chunkSent)
            {
                vTaskDelay(pdMS_TO_TICKS(1000));
                chunkSent = sendChunk(chunk);
            }
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

                String *pathPtr = new String("/09 2026/08092026.csv");
                Serial.printf("[CommandCallbacks] Queuing: %s\n", pathPtr->c_str());
                xQueueSend(bleTxQueue, &pathPtr, 0);
            }
            else if (command.startsWith("LIST"))
            {
                // int value = command.substring(4).toInt(); // "GET:" is 4 characters
                // Serial.printf("[CommandCallbacks] GET command received, generating: %d bytes\n", value);
                // String data = "\x01";

                // for (int i = 0; i < value; i++)
                //     data += "A";

                // data += "\x02";

                // String *pkgPtr = new String(data); // Allocate on heap so it survives the function scope
                // xQueueSend(bleTxQueue, &pkgPtr, 0);
                // Serial.printf("[CommandCallbacks] Sent: %u bytes\n", data.length());
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

    bleTxQueue = xQueueCreate(5, sizeof(String *));
    xTaskCreatePinnedToCore(
        sendCSVFile,
        "sendCSVFile",
        4096, // 4 KB stack
        NULL, // task parameters
        1,    // Priority (1 is standard)
        NULL, // Task handle (not needed unless deleting task)
        0     // Core ID (Core 0 is typically where WiFi/BT run)
    );

    return true;
}

bool BleServer::connected() const
{
    return clientConnected;
}
