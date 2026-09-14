#define LOGGING_SERIAL_IMPLEMENTATION
#include "Logger.h"

#include "storage/Storage.h"

#include <stdarg.h>
#include <stdio.h>

extern StorageController storage;

LoggingSerial SerialLogger(Serial);

LoggingSerial::LoggingSerial(HardwareSerial &serial) : serial(serial)
{
}

void LoggingSerial::begin(unsigned long baudRate)
{
    serial.begin(baudRate);
}

void LoggingSerial::enableStorage(bool enabled)
{
    storageEnabled = enabled;
    pendingLine = "";
}

size_t LoggingSerial::write(uint8_t byte)
{
    return write(&byte, 1);
}

size_t LoggingSerial::write(const uint8_t *buffer, size_t size)
{
    const size_t written = serial.write(buffer, size);
    appendText(buffer, size);
    return written;
}

int LoggingSerial::printf(const char *format, ...)
{
    char buffer[512];
    va_list arguments;
    va_start(arguments, format);
    const int length = vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);

    if (length <= 0)
    {
        return length;
    }

    write(reinterpret_cast<const uint8_t *>(buffer), strlen(buffer));
    return length;
}

void LoggingSerial::appendText(const uint8_t *buffer, size_t size)
{
    for (size_t index = 0; index < size; ++index)
    {
        const char character = static_cast<char>(buffer[index]);
        if (character == '\n')
        {
            flushPendingLine();
        }
        else if (character != '\r')
        {
            pendingLine += character;
        }
    }
}

void LoggingSerial::flushPendingLine()
{
    const String line = pendingLine;
    pendingLine = "";

    if (!storageEnabled || writingLog || line.length() == 0)
    {
        return;
    }

    writingLog = true;
    storage.saveLog(line);
    writingLog = false;
}