#pragma once

#include <stddef.h>
#include <stdint.h>

struct AirQualitySummary
{
    float lowPm1 = 0;
    float highPm1 = 0;
    float averagePm1 = 0;
    float medianPm1 = 0;
    float lowPm25 = 0;
    float highPm25 = 0;
    float averagePm25 = 0;
    float medianPm25 = 0;
    float lowPm10 = 0;
    float highPm10 = 0;
    float averagePm10 = 0;
    float medianPm10 = 0;
};

class AirQualityStats
{
public:
    static constexpr size_t CAPACITY = 60;

    void addSample(float pm1, float pm25, float pm10);
    bool getSummary(AirQualitySummary &summary) const;
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