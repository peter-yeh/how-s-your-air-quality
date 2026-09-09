#pragma once

#include <stddef.h>
#include <stdint.h>

class RollingWindow
{
public:
    static constexpr size_t CAPACITY = 60;

    void addSample(float pm1, float pm25, float pm10);
    bool getAverage(float &avgPm1, float &avgPm25, float &avgPm10) const;
    void clear();
    size_t getCount() const;

private:
    struct Sample
    {
        float pm1 = 0;
        float pm25 = 0;
        float pm10 = 0;
        uint32_t timestamp = 0;
    };

    Sample samples[CAPACITY];
    size_t head = 0;
    size_t count = 0;
};
