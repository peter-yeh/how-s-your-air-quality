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
    volatile bool ackReceived = false;             // Flow control: wait for client ACK
    constexpr unsigned long ACK_TIMEOUT_MS = 5000; // 5 second timeout for ACK

    // Pushes one chunk to the frontend by notifying dataCharacteristic.
    // Uses flow control: waits for ACK from client before returning.
    void sendChunk(const String &chunk)
    {
        Serial.printf("[sendChunk] Preparing to send %u bytes\n", chunk.length());
        ackReceived = false; // Reset ACK flag
        dataCharacteristic->setValue(chunk.c_str());
        const bool ok = dataCharacteristic->notify();
        if (ok)
        {
            Serial.printf("[sendChunk] Successfully sent %u bytes, waiting for ACK...\n", chunk.length());
        }
        else
        {
            Serial.printf("[sendChunk] ERROR: Failed to notify %u bytes!\n", chunk.length());
            return;
        }

        // Wait for ACK from client (with timeout)
        unsigned long startTime = millis();
        while (!ackReceived && (millis() - startTime < ACK_TIMEOUT_MS))
        {
            delay(5); // Small delay to prevent busy-waiting
        }

        if (ackReceived)
        {
            Serial.printf("[sendChunk] ACK received for %u bytes\n", chunk.length());
        }
        else
        {
            Serial.printf("[sendChunk] WARNING: No ACK received for %u bytes (timeout)\n", chunk.length());
        }
    }

    class CommandCallbacks : public NimBLECharacteristicCallbacks
    {
    public:
        void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override
        {
            const String command = characteristic->getValue().c_str();
            Serial.printf("BLE command received: %s\n", command.c_str());

            if (command == "ACK")
            {
                Serial.println("[ACK] Acknowledgment received from client");
                ackReceived = true;
                return;
            }

            if (command.startsWith("CLIENT_NAME:"))
            {
                Serial.printf("BLE client name: %s\n", command.substring(12).c_str());
                return;
            }
            if (command == "LIST")
            {
                Serial.println("[LIST] Starting LIST command processing");
                String files;
                if (!activeStorage || !activeStorage->listAllFiles(files))
                {
                    Serial.println("[LIST] ERROR: listAllFiles() failed or storage unavailable.");
                }
                Serial.printf("[LIST] Retrieved file list: %u bytes\n", files.length());
                Serial.println("[LIST] Sending LIST response...");
                sendChunk("BEGIN LIST\n" + files + "END LIST\n");
                Serial.println("[LIST] Response complete");
            }
            else if (command.startsWith("GET:"))
            {
                const String path = command.substring(4);
                Serial.printf("[GET] Starting GET command for path: %s\n", path.c_str());
                Serial.println("[GET] Sending CSV header...");
                sendChunk("BEGIN CSV\n");
                // Only the most recent readings are sent: BLE bandwidth and graph readability don't scale to entire logs.
                Serial.println("[GET] Streaming up to 300 recent lines...");
                const bool sent = activeStorage && activeStorage->streamRecentLines(path, 300, sendChunk);
                if (!sent)
                {
                    Serial.println("[GET] ERROR: Failed to stream file data");
                    sendChunk("ERROR FILE\n");
                }
                else
                {
                    Serial.println("[GET] Successfully streamed file data");
                }
                Serial.println("[GET] Sending CSV footer...");
                sendChunk("END CSV\n");
                Serial.println("[GET] Response complete");
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
