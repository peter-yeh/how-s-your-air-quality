// Dear AI AGENT, please do not touch this file without my explicit permission.
// I have spent many hours working to minimize and optimize this file. Thank you!
// Largest CSV file so far is 3MB

#include "BleServer.h"

#include <NimBLEDevice.h>
#include <SD.h>
#include "storage/Storage.h"
#include "storage/Settings.h"
#include "display/Display.h"
#include "../config/BoardConfig.h"
#define LOG_CLASS "BleServer"
#include "../utilities/Logger.h"

using namespace BleConfig;
using namespace StorageConfig;

namespace
{
    constexpr char SERVICE_UUID[] = "4fa8691a-1360-4c27-ba5c-057245417c92";
    constexpr char DATA_UUID[] = "4fa8691b-1360-4c27-ba5c-057245417c92";
    constexpr char COMMAND_UUID[] = "4fa8691c-1360-4c27-ba5c-057245417c92";

    NimBLECharacteristic *dataCharacteristic = nullptr;
    StorageController *activeStorage = nullptr;
    SettingsController *activeSettings = nullptr;
    DisplayController *activeDisplay = nullptr;
    QueueHandle_t bleTxQueue = nullptr;
    bool clientConnected = false;

    // Validate file path to prevent directory traversal attacks
    bool isValidFilePath(const String &path)
    {
        // Check for directory traversal attempts
        if (path.indexOf("..") >= 0 || path.indexOf("//") >= 0)
        {
            return false;
        }
        // Must end with .csv or .CSV
        return path.endsWith(".csv") || path.endsWith(".CSV");
    }

    bool sendChunk(const String &chunk)
    {
        dataCharacteristic->setValue(chunk.c_str());
        bool chunkQueued = dataCharacteristic->notify(reinterpret_cast<const uint8_t *>(chunk.c_str()), chunk.length());
        if (!chunkQueued)
            APP_LOG_AS("BleServer", "Failed to send %u bytes!", static_cast<unsigned>(chunk.length()));

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

            uint32_t retries = 0;
            while (!chunkSent && retries < TX_RETRY_MAX_ATTEMPTS)
            {
                vTaskDelay(pdMS_TO_TICKS(TX_RETRY_DELAY_MS));
                chunkSent = sendChunk(chunk);
                retries++;
            }

            if (!chunkSent)
            {
                APP_LOG_AS("BleServer", "Failed to send chunk after %u retries", retries);
                break;
            }
        }
    }

    void sendCSVFile(void *param)
    {
        String *filePath = nullptr;

        while (true)
        {
            if (xQueueReceive(bleTxQueue, &filePath, portMAX_DELAY) == pdPASS && filePath != nullptr)
            {
                // Validate file path
                if (!isValidFilePath(*filePath))
                {
                    APP_LOG_AS("BleServer", "Invalid file path rejected: %s", filePath->c_str());
                    sendChunk("\x03"); // Error marker
                    delete filePath;
                    continue;
                }

                // Check file size before streaming
                File file = SD.open(filePath->c_str());
                if (!file)
                {
                    APP_LOG_AS("BleServer", "File not found: %s", filePath->c_str());
                    sendChunk("\x03"); // Error marker
                    delete filePath;
                    continue;
                }

                size_t fileSize = file.size();
                file.close();

                if (fileSize > MAX_BLE_FILE_SIZE)
                {
                    APP_LOG_AS("BleServer", "File too large: %s (%u bytes > %u bytes max)",
                               filePath->c_str(), (unsigned)fileSize, (unsigned)MAX_BLE_FILE_SIZE);
                    sendChunk("\x03"); // Error marker
                    delete filePath;
                    continue;
                }

                APP_LOG_AS("BleServer", "Streaming file: %s (%u bytes)", filePath->c_str(), (unsigned)fileSize);
                sendChunk("\x01"); // Start marker
                activeStorage->streamFile(*filePath, [](const String &chunk)
                                          {
                                              uint32_t retries = 0;
                                              bool sent = sendChunk(chunk);
                                              while (!sent && retries < TX_RETRY_MAX_ATTEMPTS)
                                              {
                                                  vTaskDelay(pdMS_TO_TICKS(TX_RETRY_DELAY_MS));
                                                  sent = sendChunk(chunk);
                                                  retries++;
                                              } });
                sendChunk("\x02"); // End marker
                delete filePath;
            }
        }
    }

    class CommandCallbacks : public NimBLECharacteristicCallbacks
    {
    public:
        void onWrite(NimBLECharacteristic *characteristic, NimBLEConnInfo &) override
        {
            const String command = characteristic->getValue().c_str();

            // Limit command length to prevent buffer issues
            if (command.length() > 256)
            {
                APP_LOG_AS("CommandCallbacks", "Command rejected: too long (%u bytes)", (unsigned)command.length());
                return;
            }

            APP_LOG_AS("CommandCallbacks", "Command received: %s", command.c_str());

            if (command.startsWith("GET:"))
            {
                String filename = command.substring(4);

                // Validate filename before queuing
                if (!isValidFilePath(filename))
                {
                    APP_LOG_AS("CommandCallbacks", "GET rejected: invalid path %s", filename.c_str());
                    sendChunk("\x03"); // Error marker
                    return;
                }

                String *pathPtr = new String(filename);
                if (xQueueSend(bleTxQueue, &pathPtr, 0) != pdPASS)
                {
                    APP_LOG_AS("CommandCallbacks", "Queue send failed, dropping request");
                    delete pathPtr; // Prevent leak on failed send
                }
                else
                {
                    APP_LOG_AS("CommandCallbacks", "Streaming CSV file: %s", pathPtr->c_str());
                }
            }
            else if (command.startsWith("LIST"))
            {
                String fileList;
                if (activeStorage->listCsvFiles(fileList))
                {
                    APP_LOG_AS("CommandCallbacks", "Sending file list (%u bytes)", (unsigned)fileList.length());
                    sendPackage("\x01" + fileList + "\x02");
                }
                else
                {
                    APP_LOG_AS("CommandCallbacks", "Failed to list CSV files");
                    sendChunk("\x03"); // Error marker
                }
            }
            else if (command.startsWith("GetSettings"))
            {
                uint8_t brightness = activeSettings->getBrightness();
                uint8_t graphMode = activeSettings->getGraphMode();
                String response = "SETTINGS:" + String(brightness) + "," + String(graphMode);
                APP_LOG_AS("CommandCallbacks", "Sending settings: brightness=%u, graphMode=%u", brightness, graphMode);
                sendChunk(response);
            }
            else if (command.startsWith("GetBrightness"))
            {
                uint8_t brightness = activeSettings->getBrightness();
                String response = "BRIGHTNESS:" + String(brightness);
                APP_LOG_AS("CommandCallbacks", "Sending legacy brightness: %u", brightness);
                sendChunk(response);
            }
            else if (command.startsWith("SetBrightness:"))
            {
                String brightnessStr = command.substring(14);
                int brightnessVal = brightnessStr.toInt();

                // Validate range
                if (brightnessStr.length() == 0 || brightnessVal < 0 || brightnessVal > 255)
                {
                    APP_LOG_AS("CommandCallbacks", "SetBrightness rejected: invalid value '%s'", brightnessStr.c_str());
                    sendChunk("\x03"); // Error marker
                    return;
                }

                uint8_t brightness = (uint8_t)brightnessVal;
                if (activeSettings->setBrightness(brightness))
                {
                    if (activeDisplay)
                    {
                        activeDisplay->setBrightness(brightness);
                        APP_LOG_AS("CommandCallbacks", "Brightness set to %u", brightness);
                    }
                }
                else
                {
                    APP_LOG_AS("CommandCallbacks", "Failed to save brightness %u", brightness);
                    sendChunk("\x03"); // Error marker
                }
            }
            else if (command.startsWith("SetGraphMode:"))
            {
                String modeStr = command.substring(13);
                int modeVal = modeStr.toInt();

                // Validate range
                if (modeStr.length() == 0 || modeVal < 0 || modeVal > 2)
                {
                    APP_LOG_AS("CommandCallbacks", "SetGraphMode rejected: invalid value '%s'", modeStr.c_str());
                    sendChunk("\x03"); // Error marker
                    return;
                }

                uint8_t mode = (uint8_t)modeVal;
                if (activeSettings->setGraphMode(mode) && activeDisplay)
                {
                    activeDisplay->setGraphMode(mode);
                    APP_LOG_AS("CommandCallbacks", "Graph mode set to %u", mode);
                }
                else
                {
                    APP_LOG_AS("CommandCallbacks", "Failed to save graph mode %u", mode);
                    sendChunk("\x03"); // Error marker
                }
            }
            else
            {
                APP_LOG_AS("CommandCallbacks", "Unknown command: %s", command.c_str());
                sendChunk("\x03"); // Error marker
            }
        }
    };

    class ServerCallbacks : public NimBLEServerCallbacks
    {
    public:
        void onConnect(NimBLEServer *, NimBLEConnInfo &connInfo) override
        {
            clientConnected = true;
            APP_LOG_AS("ServerCallbacks", "BLE client connected, peer address: %s, MTU: %u",
                       connInfo.getAddress().toString().c_str(),
                       static_cast<unsigned>(connInfo.getMTU()));
            NimBLEDevice::stopAdvertising();
        }

        void onDisconnect(NimBLEServer *, NimBLEConnInfo &connInfo, int reason) override
        {
            clientConnected = false;
            APP_LOG_AS("ServerCallbacks", "BLE client disconnected, peer address: %s, reason: %d",
                       connInfo.getAddress().toString().c_str(),
                       reason);
            NimBLEDevice::startAdvertising();
        }
    };
}

bool BleServer::begin(StorageController *storage, SettingsController *settings, DisplayController *display)
{
    activeStorage = storage;
    activeSettings = settings;
    activeDisplay = display;

    NimBLEDevice::init("Air Quality Monitor");
    // NimBLEDevice::setMTU(MAX_TRANSMISSION_UNIT);
    NimBLEServer *server = NimBLEDevice::createServer();
    server->setCallbacks(new ServerCallbacks());
    NimBLEService *service = server->createService(SERVICE_UUID);
    dataCharacteristic = service->createCharacteristic(DATA_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    NimBLECharacteristic *commandCharacteristic = service->createCharacteristic(COMMAND_UUID, NIMBLE_PROPERTY::WRITE);
    commandCharacteristic->setCallbacks(new CommandCallbacks());
    dataCharacteristic->setValue("Air Quality Monitor ready");
    server->start();

    NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
    advertising->enableScanResponse(true);
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->setName("Air Quality Monitor");
    advertising->start();
    APP_LOG("BLE advertising as Air Quality Monitor.");

    bleTxQueue = xQueueCreate(TX_QUEUE_SIZE, sizeof(String *));
    if (bleTxQueue == nullptr)
    {
        APP_LOG("Failed to create BLE TX queue");
        return false;
    }

    xTaskCreatePinnedToCore(
        sendCSVFile,
        "sendCSVFile",
        TX_TASK_STACK_SIZE,
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
