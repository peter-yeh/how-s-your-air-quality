#pragma once

#include <Arduino.h>

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
    void appendText(const uint8_t *buffer, size_t size);
    void flushPendingLine();

    HardwareSerial &serial;
    String pendingLine;
    bool storageEnabled = false;
    bool writingLog = false;
};

extern LoggingSerial SerialLogger;

#ifndef LOGGING_SERIAL_IMPLEMENTATION
#define Serial SerialLogger
#endif