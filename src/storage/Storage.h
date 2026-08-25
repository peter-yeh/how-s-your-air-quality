#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SPI.h>

class StorageController
{
public:
    StorageController();
    bool begin();
    bool testReadWrite();
    bool saveToCsv(const String &data);

private:
    void printDirectory(fs::FS &filesystem, const char *path);

    SPIClass sdSpi;
    bool initialized = false;
};
