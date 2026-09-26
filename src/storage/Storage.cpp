#include "Storage.h"

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <time.h>
#define LOG_CLASS "StorageController"
#include "../utilities/Logger.h"
#include "../config/BoardConfig.h"

using namespace StorageConfig;

bool isCsvFile(const String &name)
{
    return name.endsWith(".csv") || name.endsWith(".CSV");
}

String normalizePath(String dir, String filename)
{
    String fullPath = "";
    if (dir.length() > 0)
    {
        if (!dir.startsWith("/"))
            dir = "/" + dir;
        if (dir.endsWith("/"))
            dir = dir.substring(0, dir.length() - 1);
        fullPath += dir;
    }
    if (!filename.startsWith("/"))
        filename = "/" + filename;
    fullPath += filename;
    return fullPath;
}

class StorageLock
{
public:
    explicit StorageLock(SemaphoreHandle_t mutex) : mutex(mutex)
    {
        locked = mutex != nullptr && xSemaphoreTake(mutex, portMAX_DELAY) == pdPASS;
    }

    ~StorageLock()
    {
        if (locked)
        {
            xSemaphoreGive(mutex);
        }
    }

    bool acquired() const
    {
        return locked;
    }

private:
    SemaphoreHandle_t mutex;
    bool locked = false;
};

StorageController::StorageController() : sdSpi(HSPI)
{
}

bool StorageController::begin()
{
    if (storageMutex == nullptr)
    {
        storageMutex = xSemaphoreCreateMutex();
    }

    if (storageMutex == nullptr)
    {
        APP_LOG("Storage mutex initialization failed.");
        return false;
    }

    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, sdSpi, SD_FREQUENCY))
    {
        APP_LOG("SD card initialization failed.");
        return false;
    }

    initialized = true;

    APP_LOG("SD card initialized.");
    APP_LOG("SD card size: %u MB", static_cast<unsigned>(SD.cardSize() / (1024 * 1024)));
    return true;
}

bool StorageController::saveToCsv(const String &data)
{
    StorageLock lock(storageMutex);
    if (!lock.acquired())
    {
        return false;
    }

    if (!initialized)
    {
        APP_LOG("Cannot save CSV: SD card is not initialized.");
        return false;
    }

    time_t now = time(nullptr);
    struct tm currentTime;
    localtime_r(&now, &currentTime);
    if (currentTime.tm_year < 120)
    {
        APP_LOG("Cannot save CSV: system clock is not set.");
        return false;
    }

    char monthFolder[16];
    char filename[24];
    snprintf(monthFolder, sizeof(monthFolder), "/%02d %04d",
             currentTime.tm_mon + 1, currentTime.tm_year + 1900);
    snprintf(filename, sizeof(filename), "%s/%02d%02d%04d.csv",
             monthFolder, currentTime.tm_mday, currentTime.tm_mon + 1,
             currentTime.tm_year + 1900);

    if (!SD.exists(monthFolder) && !SD.mkdir(monthFolder))
    {
        APP_LOG("Cannot create folder: %s", monthFolder);
        return false;
    }

    File csvFile = SD.open(filename, FILE_APPEND);
    if (!csvFile)
    {
        APP_LOG("Cannot open CSV file: %s (file open failed)", filename);
        return false;
    }

    size_t written = csvFile.println(data);
    bool success = (csvFile.getWriteError() == 0) && (written > 0);
    csvFile.close();

    if (!success)
    {
        APP_LOG("Cannot write CSV row: %s (write error)", filename);
    }
    else
    {
        APP_LOG("CSV row written: %s", data.c_str());
    }
    return success;
}

bool StorageController::saveLogBatch(const char *data, size_t length)
{
    if (data == nullptr || length == 0)
    {
        return true;
    }

    StorageLock lock(storageMutex);
    if (!lock.acquired())
    {
        return false;
    }

    if (!initialized)
    {
        APP_LOG("Cannot save log: SD card is not initialized.");
        return false;
    }

    time_t now = time(nullptr);
    struct tm currentTime;
    localtime_r(&now, &currentTime);
    if (currentTime.tm_year < 120)
    {
        APP_LOG("Cannot save log: system clock is not set properly.");
        return false;
    }

    char monthFolder[16];
    char filename[24];
    snprintf(monthFolder, sizeof(monthFolder), "/%02d %04d",
             currentTime.tm_mon + 1, currentTime.tm_year + 1900);
    snprintf(filename, sizeof(filename), "%s/%02d%02d%04d.log",
             monthFolder, currentTime.tm_mday, currentTime.tm_mon + 1,
             currentTime.tm_year + 1900);

    if (!SD.exists(monthFolder) && !SD.mkdir(monthFolder))
    {
        APP_LOG("Cannot create log folder: %s", monthFolder);
        return false;
    }

    File logFile = SD.open(filename, FILE_APPEND);
    if (!logFile)
    {
        APP_LOG("Cannot open log file: %s (file open failed)", filename);
        return false;
    }

    const size_t bytesWritten = logFile.write(reinterpret_cast<const uint8_t *>(data), length);
    const bool written = (bytesWritten == length) && (logFile.getWriteError() == 0);
    logFile.close();

    if (!written)
    {
        APP_LOG("Cannot write log file: %s (wrote %u/%u bytes)", filename, (unsigned)bytesWritten, (unsigned)length);
    }
    return written;
}

bool StorageController::saveReading(const Reading &reading)
{
    if (reading.time.length() == 0 || reading.time == "time unavailable" || reading.time == "--:--:--")
    {
        APP_LOG("Cannot save CSV: reading has no valid time.");
        return false;
    }

    String csvRow;
    csvRow.reserve(128);
    csvRow += reading.time;
    csvRow += ",";
    csvRow += String(reading.pm1, 2);
    csvRow += ",";
    csvRow += String(reading.pm25, 2);
    csvRow += ",";
    csvRow += String(reading.pm10, 2);
    csvRow += ",";
    csvRow += String(reading.c02, 2);
    csvRow += ",";
    csvRow += String(reading.humidity, 2);
    csvRow += ",";
    csvRow += String(reading.pressure, 2);
    csvRow += ",";
    csvRow += String(reading.altitude, 2);
    csvRow += ",";
    csvRow += String(reading.xCoord, 2);
    csvRow += ",";
    csvRow += String(reading.yCoord, 2);

    return saveToCsv(csvRow);
}

bool StorageController::listCsvFiles(String &result)
{
    StorageLock lock(storageMutex);
    if (!lock.acquired())
    {
        return false;
    }

    if (!initialized)
    {
        return false;
    }

    result = "";
    File root = SD.open("/");
    if (!root || !root.isDirectory())
    {
        return false;
    }

    File entry = root.openNextFile();
    while (entry)
    {
        if (entry.isDirectory())
        {
            String dirName = String(entry.name());
            if (!dirName.endsWith("System Volume Information"))
            {
                File child = entry.openNextFile();
                while (child)
                {
                    if (!child.isDirectory() && isCsvFile(String(child.name())))
                    {
                        result += normalizePath(dirName, String(child.name()));
                        result += "|";
                        result += String(child.size());
                        result += "\n";
                    }
                    child = entry.openNextFile();
                }
            }
        }
        else if (isCsvFile(String(entry.name())))
        {
            result += normalizePath("", String(entry.name()));
            result += "|";
            result += String(entry.size());
            result += "\n";
        }
        entry = root.openNextFile();
    }
    return true;
}

bool StorageController::streamFile(const String &path, void (*onChunk)(const String &))
{
    StorageLock lock(storageMutex);
    if (!lock.acquired())
    {
        return false;
    }

    if (!initialized || !onChunk || !path.startsWith("/") || path.indexOf("..") >= 0)
    {
        return false;
    }

    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory())
    {
        APP_LOG("streamFile: cannot open %s", path.c_str());
        return false;
    }

    APP_LOG("streamFile: opened %s, size %u bytes", path.c_str(), static_cast<unsigned>(file.size()));
    char buffer[245]; // 244 bytes + 1 null terminator
    size_t chunkIndex = 0;
    while (file.available())
    {
        const size_t count = file.readBytes(buffer, sizeof(buffer) - 1);
        buffer[count] = '\0';
        onChunk(String(buffer));
        chunkIndex++;
    }
    APP_LOG("streamFile: done, %u chunk(s) sent", static_cast<unsigned>(chunkIndex));
    file.close();
    return true;
}
