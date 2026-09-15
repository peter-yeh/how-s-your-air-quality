#include "Storage.h"

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <time.h>
#include <Preferences.h>
#include "../utilities/Logger.h"

namespace
{
    constexpr uint8_t SD_CS = 5;
    constexpr uint8_t SD_SCK = 18;
    constexpr uint8_t SD_MOSI = 23;
    constexpr uint8_t SD_MISO = 19;
    constexpr uint32_t SD_FREQUENCY = 20000000;

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
}

StorageController::StorageController() : sdSpi(HSPI)
{
}

void StorageController::printDirectory(fs::FS &filesystem, const char *path)
{
    File directory = filesystem.open(path);
    if (!directory || !directory.isDirectory())
    {
        Serial.println("Unable to open SD root directory.");
        return;
    }

    File entry = directory.openNextFile();
    if (!entry)
    {
        Serial.println("SD root directory is empty.");
    }

    while (entry)
    {
        Serial.print(entry.isDirectory() ? "DIR  " : "FILE ");
        Serial.print(entry.name());
        if (!entry.isDirectory())
        {
            Serial.print("  ");
            Serial.print(entry.size());
            Serial.print(" bytes");
        }
        Serial.println();
        entry = directory.openNextFile();
    }
}

bool StorageController::begin()
{
    if (storageMutex == nullptr)
    {
        storageMutex = xSemaphoreCreateMutex();
    }

    if (storageMutex == nullptr)
    {
        Serial.println("Storage mutex initialization failed.");
        return false;
    }

    Preferences preferences;
    if (preferences.begin("air_sensor", true))
    {
        brightness = preferences.getUChar("brightness", brightness);
        const uint8_t storedGraphMode = preferences.getUChar("graphMode", graphMode);
        if (storedGraphMode <= 2)
        {
            graphMode = storedGraphMode;
        }
        preferences.end();
    }

    pinMode(SD_CS, OUTPUT);
    digitalWrite(SD_CS, HIGH);

    sdSpi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, sdSpi, SD_FREQUENCY))
    {
        Serial.println("SD card initialization failed.");
        return false;
    }

    initialized = true;

    Serial.println("SD card initialized.");
    Serial.print("SD card size: ");
    Serial.print(SD.cardSize() / (1024 * 1024));
    Serial.println(" MB");
    printDirectory(SD, "/");
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
        Serial.println("Cannot save CSV: SD card is not initialized.");
        return false;
    }

    time_t now = time(nullptr);
    struct tm currentTime;
    localtime_r(&now, &currentTime);
    if (currentTime.tm_year < 120)
    {
        Serial.println("Cannot save CSV: system clock is not set.");
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
        Serial.print("Cannot create folder: ");
        Serial.println(monthFolder);
        return false;
    }

    File csvFile = SD.open(filename, FILE_APPEND);
    if (!csvFile)
    {
        Serial.print("Cannot open CSV file: ");
        Serial.println(filename);
        return false;
    }

    csvFile.println(data);
    bool written = csvFile.getWriteError() == 0;
    csvFile.close();

    if (!written)
    {
        Serial.print("Cannot write CSV row: ");
        Serial.println(filename);
    }
    return written;
}

bool StorageController::saveLog(const String &message)
{
    return saveLogBatch(message.c_str(), message.length());
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
        Serial.println("Cannot save log: SD card is not initialized.");
        return false;
    }

    time_t now = time(nullptr);
    struct tm currentTime;
    localtime_r(&now, &currentTime);
    if (currentTime.tm_year < 120)
    {
        Serial.println("Cannot save log: system clock is not set.");
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
        Serial.print("Cannot create log folder: ");
        Serial.println(monthFolder);
        return false;
    }

    File logFile = SD.open(filename, FILE_APPEND);
    if (!logFile)
    {
        Serial.print("Cannot open log file: ");
        Serial.println(filename);
        return false;
    }

    char timestamp[24];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &currentTime);

    size_t lineCount = 1;
    for (size_t index = 0; index < length; ++index)
    {
        if (data[index] == '\n')
        {
            ++lineCount;
        }
    }

    String formattedBatch;
    formattedBatch.reserve(length + lineCount * 24);

    const char *lineStart = data;
    const char *dataEnd = data + length;
    while (lineStart < dataEnd)
    {
        const char *lineEnd = lineStart;
        while (lineEnd < dataEnd && *lineEnd != '\n')
        {
            ++lineEnd;
        }

        size_t lineLength = static_cast<size_t>(lineEnd - lineStart);
        while (lineLength > 0 && lineStart[lineLength - 1] == '\r')
        {
            --lineLength;
        }

        if (lineLength > 0)
        {
            formattedBatch += '[';
            formattedBatch += timestamp;
            formattedBatch += "] ";
            formattedBatch.concat(lineStart, lineLength);
            formattedBatch += '\n';
        }

        if (lineEnd == dataEnd)
        {
            break;
        }
        lineStart = lineEnd + 1;
    }

    const size_t bytesWritten = logFile.write(
        reinterpret_cast<const uint8_t *>(formattedBatch.c_str()),
        formattedBatch.length());
    const bool written = bytesWritten == formattedBatch.length() && logFile.getWriteError() == 0;
    logFile.close();

    if (!written)
    {
        Serial.print("Cannot write log file: ");
        Serial.println(filename);
    }
    return written;
}

bool StorageController::saveReading(const Reading &reading)
{
    if (reading.time.length() == 0 || reading.time == "time unavailable" || reading.time == "--:--:--")
    {
        Serial.println("Cannot save CSV: reading has no valid time.");
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

bool StorageController::listAllFiles(String &result)
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
        const String entryName = String(entry.name());
        if (entry.isDirectory())
        {
            if (entryName != "System Volume Information")
            {
                File child = entry.openNextFile();
                while (child)
                {
                    if (!child.isDirectory())
                    {
                        result += "/" + entryName + "/" + String(child.name());
                        result += "\n";
                    }
                    child = entry.openNextFile();
                }
            }
        }
        else
        {
            result += "/" + entryName;
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
        Serial.printf("streamFile: cannot open %s\n", path.c_str());
        return false;
    }

    Serial.printf("streamFile: opened %s, size %u bytes\n", path.c_str(), (unsigned)file.size());
    char buffer[245]; // 244 bytes + 1 null terminator
    size_t chunkIndex = 0;
    while (file.available())
    {
        const size_t count = file.readBytes(buffer, sizeof(buffer) - 1);
        buffer[count] = '\0';
        onChunk(String(buffer));
        chunkIndex++;
    }
    Serial.printf("streamFile: done, %u chunk(s) sent\n", (unsigned)chunkIndex);
    file.close();
    return true;
}

bool StorageController::streamRecentLines(const String &path, size_t maxLines, void (*onChunk)(const String &))
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
        Serial.printf("streamRecentLines: cannot open %s\n", path.c_str());
        return false;
    }

    // CSV rows are ~60 bytes; seek near the end instead of reading the whole file.
    constexpr size_t AVG_LINE_BYTES = 64;
    const size_t fileSize = file.size();
    const size_t approxBytes = maxLines * AVG_LINE_BYTES;
    const size_t startPos = fileSize > approxBytes ? fileSize - approxBytes : 0;

    if (startPos > 0)
    {
        file.seek(startPos);
        while (file.available() && file.read() != '\n')
        {
        }
    }

    Serial.printf("streamRecentLines: %s, size %u bytes, starting at %u\n", path.c_str(), (unsigned)fileSize, (unsigned)file.position());
    char buffer[181];
    size_t chunkIndex = 0;
    while (file.available())
    {
        const size_t count = file.readBytes(buffer, sizeof(buffer) - 1);
        buffer[count] = '\0';
        onChunk(String(buffer));
        chunkIndex++;
    }
    Serial.printf("streamRecentLines: done, %u chunk(s) sent\n", (unsigned)chunkIndex);
    file.close();
    return true;
}

bool StorageController::testReadWrite()
{
    StorageLock lock(storageMutex);
    if (!lock.acquired())
    {
        return false;
    }

    constexpr char TEST_FILE[] = "/sd_test.txt";
    const String expected = "ESP32 SD read/write test";

    if (!initialized)
    {
        Serial.println("SD read/write test skipped: card is not initialized.");
        return false;
    }

    File file = SD.open(TEST_FILE, FILE_WRITE);
    if (!file)
    {
        Serial.println("SD write test failed: cannot open test file.");
        return false;
    }
    file.println(expected);
    bool writeSucceeded = file.getWriteError() == 0;
    file.close();

    if (!writeSucceeded)
    {
        Serial.println("SD write test failed.");
        return false;
    }

    file = SD.open(TEST_FILE, FILE_READ);
    if (!file)
    {
        Serial.println("SD read test failed: cannot open test file.");
        return false;
    }
    String actual = file.readStringUntil('\n');
    file.close();
    actual.trim();

    bool passed = actual == expected;
    Serial.println(passed ? "SD read/write test passed." : "SD read/write test failed: data mismatch.");
    return passed;
}

uint8_t StorageController::getBrightness()
{
    Serial.printf("getBrightness: %u\n", brightness);
    return brightness;
}

bool StorageController::setBrightness(uint8_t brightness)
{
    Preferences preferences;
    preferences.begin("air_sensor", false); // read-write mode
    preferences.putUChar("brightness", brightness);
    bool success = preferences.getBytesLength("brightness") > 0;
    preferences.end();
    if (success)
    {
        this->brightness = brightness;
    }
    Serial.printf("setBrightness: %u (success: %s)\n", brightness, success ? "true" : "false");
    return success;
}

uint8_t StorageController::getGraphMode()
{
    Serial.printf("getGraphMode: %u\n", graphMode);
    return graphMode;
}

bool StorageController::setGraphMode(uint8_t graphMode)
{
    if (graphMode > 2)
    {
        Serial.printf("setGraphMode: invalid mode %u\n", graphMode);
        return false;
    }

    Preferences preferences;
    if (!preferences.begin("air_sensor", false))
    {
        Serial.println("setGraphMode: unable to open preferences");
        return false;
    }

    preferences.putUChar("graphMode", graphMode);
    const bool success = preferences.getUChar("graphMode", 255) == graphMode;
    preferences.end();

    if (success)
    {
        this->graphMode = graphMode;
    }

    Serial.printf("setGraphMode: %u (success: %s)\n", graphMode, success ? "true" : "false");
    return success;
}
