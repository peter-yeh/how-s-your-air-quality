#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

class LoggingSerial : public Print
{
public:
    explicit LoggingSerial(HardwareSerial &serial);

    void begin(unsigned long baudRate);
    void enableStorage(bool enabled);
    size_t write(uint8_t byte) override;
    size_t write(const uint8_t *buffer, size_t size) override;
    int printf(const char *format, ...);

private:
    static constexpr size_t LOG_LINE_CAPACITY = 512;
    static constexpr size_t LOG_BATCH_CAPACITY = 4096;
    static constexpr size_t LOG_BATCH_COUNT = 3;
    static constexpr size_t LOG_BATCH_FLUSH_SIZE = 3072;
    static constexpr uint32_t LOG_BATCH_FLUSH_INTERVAL_MS = 100;

    struct LogBatch
    {
        size_t length = 0;
        char data[LOG_BATCH_CAPACITY];
    };

    static void storageTaskEntry(void *parameter);
    void storageTaskLoop();
    void appendText(const uint8_t *buffer, size_t size);
    void appendPendingLine();
    void queueActiveBatch();
    void flushActiveBatch();

    HardwareSerial &serial;
    char pendingLine[LOG_LINE_CAPACITY] = {};
    size_t pendingLineLength = 0;
    bool storageEnabled = false;
    volatile bool writingLog = false;
    SemaphoreHandle_t stateMutex = nullptr;
    QueueHandle_t freeBatchQueue = nullptr;
    QueueHandle_t readyBatchQueue = nullptr;
    TaskHandle_t storageTask = nullptr;
    LogBatch batchPool[LOG_BATCH_COUNT];
    LogBatch *activeBatch = nullptr;
};

extern LoggingSerial SerialLogger;

#ifndef LOGGING_SERIAL_IMPLEMENTATION
#define Serial SerialLogger
#endif