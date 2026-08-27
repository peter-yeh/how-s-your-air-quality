#include "Storage.h"

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include <time.h>

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
            File child = entry.openNextFile();
            while (child)
            {
                if (!child.isDirectory() && isCsvFile(String(child.name())))
                {
                    result += String(child.name());
                    result += "\n";
                }
                child = entry.openNextFile();
            }
        }
        else if (isCsvFile(String(entry.name())))
        {
            result += String(entry.name());
            result += "\n";
        }
        entry = root.openNextFile();
    }
    return true;
}

bool StorageController::streamFile(const String &path, void (*onChunk)(const String &, void *), void *context)
{
    if (!initialized || !onChunk || !path.startsWith("/") || path.indexOf("..") >= 0)
    {
        return false;
    }

    File file = SD.open(path, FILE_READ);
    if (!file || file.isDirectory())
    {
        return false;
    }

    char buffer[181];
    while (file.available())
    {
        const size_t count = file.readBytes(buffer, sizeof(buffer) - 1);
        buffer[count] = '\0';
        onChunk(String(buffer), context);
    }
    file.close();
    return true;
}

bool StorageController::testReadWrite()
{
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
