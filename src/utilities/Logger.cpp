#define LOG_CLASS "LoggingSerial"
#define LOGGING_SERIAL_IMPLEMENTATION
#include "Logger.h"

#include "../storage/Storage.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

extern StorageController storage;

LoggingSerial SerialLogger(Serial);

namespace
{
    const char *fileNameOnly(const char *path)
    {
        if (path == nullptr)
        {
            return "unknown";
        }

        const char *slash = strrchr(path, '/');
        const char *backslash = strrchr(path, '\\');
        if (backslash != nullptr && (slash == nullptr || backslash > slash))
        {
            slash = backslash;
        }
        return slash == nullptr ? path : slash + 1;
    }
}

LoggingSerial::LoggingSerial(HardwareSerial &serial) : serial(serial)
{
}

void LoggingSerial::begin(unsigned long baudRate)
{
    serial.begin(baudRate);
}

void LoggingSerial::enableStorage(bool enabled)
{
    if (!enabled)
    {
        storageEnabled = false;
        pendingLineLength = 0;
        return;
    }

    if (storageTask != nullptr)
    {
        storageEnabled = true;
        return;
    }

    stateMutex = xSemaphoreCreateMutex();
    freeBatchQueue = xQueueCreate(LOG_BATCH_COUNT, sizeof(LogBatch *));
    readyBatchQueue = xQueueCreate(LOG_BATCH_COUNT, sizeof(LogBatch *));

    if (stateMutex == nullptr || freeBatchQueue == nullptr || readyBatchQueue == nullptr)
    {
        APP_LOG("Logger initialization failed.");
        return;
    }

    for (LogBatch &batch : batchPool)
    {
        batch.length = 0;
        LogBatch *batchPointer = &batch;
        xQueueSend(freeBatchQueue, &batchPointer, 0);
    }

    if (xQueueReceive(freeBatchQueue, &activeBatch, 0) != pdPASS)
    {
        APP_LOG("Logger buffer initialization failed.");
        activeBatch = nullptr;
        return;
    }

    storageEnabled = true;
    if (xTaskCreatePinnedToCore(storageTaskEntry, "LoggerTask", 6144, this, 1, &storageTask, 1) != pdPASS)
    {
        storageEnabled = false;
        storageTask = nullptr;
        APP_LOG("Logger task creation failed.");
    }
}

size_t LoggingSerial::write(uint8_t byte)
{
    return write(&byte, 1);
}

size_t LoggingSerial::write(const uint8_t *buffer, size_t size)
{
    const size_t written = serial.write(buffer, size);
    if (storageEnabled && !writingLog)
    {
        appendText(buffer, size);
    }
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

void LoggingSerial::log(const char *filename, const char *className, const char *functionName, const char *format, ...)
{
    char timestamp[24] = "time unavailable";
    const time_t now = time(nullptr);
    struct tm currentTime = {};
    if (now != static_cast<time_t>(-1) &&
        localtime_r(&now, &currentTime) != nullptr &&
        currentTime.tm_year >= 120)
    {
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &currentTime);
    }

    const char *safeClassName = className != nullptr ? className : "Unknown";
    const char *safeFunctionName = functionName != nullptr ? functionName : "unknown";
    const char *safeFormat = format != nullptr ? format : "";
    char formattedLine[LOG_LINE_CAPACITY];
    const int prefixLength = snprintf(
        formattedLine,
        sizeof(formattedLine),
        "%-20s | %-20s | %-20s | %-20s | ",
        timestamp,
        fileNameOnly(filename),
        safeClassName,
        safeFunctionName);

    if (prefixLength < 0 || static_cast<size_t>(prefixLength) >= sizeof(formattedLine) - 1)
    {
        return;
    }

    const size_t prefixSize = static_cast<size_t>(prefixLength);
    const size_t bodyCapacity = sizeof(formattedLine) - prefixSize - 2;
    va_list arguments;
    va_start(arguments, format);
    const int bodyLength = vsnprintf(formattedLine + prefixSize, bodyCapacity + 1, safeFormat, arguments);
    va_end(arguments);

    if (bodyLength < 0)
    {
        return;
    }

    size_t bodySize = static_cast<size_t>(bodyLength);
    if (bodySize > bodyCapacity)
    {
        bodySize = bodyCapacity;
    }

    for (size_t index = 0; index < bodySize; ++index)
    {
        if (formattedLine[prefixSize + index] == '\r' || formattedLine[prefixSize + index] == '\n')
        {
            formattedLine[prefixSize + index] = ' ';
        }
    }

    const size_t lineLength = prefixSize + bodySize;
    formattedLine[lineLength] = '\n';
    write(reinterpret_cast<const uint8_t *>(formattedLine), lineLength + 1);
}

void LoggingSerial::appendText(const uint8_t *buffer, size_t size)
{
    if (stateMutex == nullptr || xSemaphoreTake(stateMutex, portMAX_DELAY) != pdPASS)
    {
        return;
    }

    for (size_t index = 0; index < size; ++index)
    {
        const char character = static_cast<char>(buffer[index]);
        if (character == '\n')
        {
            appendPendingLine();
        }
        else if (character != '\r')
        {
            if (pendingLineLength < LOG_LINE_CAPACITY - 1)
            {
                pendingLine[pendingLineLength++] = character;
            }
        }
    }

    xSemaphoreGive(stateMutex);
}

void LoggingSerial::appendPendingLine()
{
    if (pendingLineLength == 0)
    {
        return;
    }

    if (activeBatch == nullptr && xQueueReceive(freeBatchQueue, &activeBatch, 0) != pdPASS)
    {
        pendingLineLength = 0;
        return;
    }

    const size_t requiredLength = pendingLineLength + 1;
    if (activeBatch->length + requiredLength > LOG_BATCH_CAPACITY)
    {
        queueActiveBatch();
    }

    if (activeBatch == nullptr || activeBatch->length + requiredLength > LOG_BATCH_CAPACITY)
    {
        pendingLineLength = 0;
        return;
    }

    memcpy(activeBatch->data + activeBatch->length, pendingLine, pendingLineLength);
    activeBatch->length += pendingLineLength;
    activeBatch->data[activeBatch->length++] = '\n';
    pendingLineLength = 0;

    if (activeBatch->length >= LOG_BATCH_FLUSH_SIZE)
    {
        queueActiveBatch();
    }
}

void LoggingSerial::queueActiveBatch()
{
    if (activeBatch == nullptr || activeBatch->length == 0)
    {
        return;
    }

    LogBatch *batch = activeBatch;
    if (xQueueSend(readyBatchQueue, &batch, 0) != pdPASS)
    {
        return;
    }

    activeBatch = nullptr;
    LogBatch *replacement = nullptr;
    if (xQueueReceive(freeBatchQueue, &replacement, 0) == pdPASS)
    {
        replacement->length = 0;
        activeBatch = replacement;
    }
}

void LoggingSerial::flushActiveBatch()
{
    if (stateMutex == nullptr || xSemaphoreTake(stateMutex, 0) != pdPASS)
    {
        return;
    }

    queueActiveBatch();
    xSemaphoreGive(stateMutex);
}

void LoggingSerial::storageTaskEntry(void *parameter)
{
    static_cast<LoggingSerial *>(parameter)->storageTaskLoop();
}

void LoggingSerial::storageTaskLoop()
{
    while (true)
    {
        LogBatch *batch = nullptr;
        const BaseType_t received = xQueueReceive(
            readyBatchQueue,
            &batch,
            pdMS_TO_TICKS(LOG_BATCH_FLUSH_INTERVAL_MS));

        if (received == pdPASS)
        {
            if (batch != nullptr && batch->length > 0 && storageEnabled)
            {
                writingLog = true;
                storage.saveLogBatch(batch->data, batch->length);
                writingLog = false;
            }

            if (batch != nullptr)
            {
                batch->length = 0;
                xQueueSend(freeBatchQueue, &batch, portMAX_DELAY);
            }
        }
        else
        {
            flushActiveBatch();
        }
    }
}