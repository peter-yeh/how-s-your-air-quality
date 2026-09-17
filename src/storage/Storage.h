#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>
#include <time.h>
#include <freertos/semphr.h>

struct Reading
{
    String time;
    float pm1 = 0;
    float pm25 = 0;
    float pm10 = 0;
    float c02 = 0;
    float humidity = 0;
    float pressure = 0;
    float altitude = 0;
    float xCoord = 0;
    float yCoord = 0;
};

class StorageController
{
public:
    StorageController();
    bool begin();
    void createNextDayFile();
    bool testReadWrite();
    bool saveToCsv(const String &data);
    bool saveLog(const String &message);
    bool saveLogBatch(const char *data, size_t length);
    bool saveReading(const Reading &reading);
    bool listCsvFiles(String &result);
    bool listAllFiles(String &result);
    bool streamFile(const String &path, void (*onChunk)(const String &));
    bool streamRecentLines(const String &path, size_t maxLines, void (*onChunk)(const String &));

private:
    void createDailyFiles(const struct tm &date);
    void printDirectory(fs::FS &filesystem, const char *path);

    SPIClass sdSpi;
    bool initialized = false;
    bool isNextDayFileCreated = false;
    SemaphoreHandle_t storageMutex = nullptr;
};
