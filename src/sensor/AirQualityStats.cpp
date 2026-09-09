#include "AirQualityStats.h"
#include <Arduino.h>
#include <algorithm>

namespace
{
    float median(float *sorted, size_t n)
    {
        std::sort(sorted, sorted + n);
        if (n % 2 == 1)
        {
            return sorted[n / 2];
        }
        return (sorted[n / 2 - 1] + sorted[n / 2]) / 2.0f;
    }
}

void AirQualityStats::addSample(float pm1, float pm25, float pm10)
{
    samples[head].pm1 = pm1;
    samples[head].pm25 = pm25;
    samples[head].pm10 = pm10;
    samples[head].timestamp = millis();
    head = (head + 1) % CAPACITY;
    if (count < CAPACITY)
    {
        count++;
    }
}

bool AirQualityStats::getSummary(AirQualitySummary &summary) const
{
    if (count == 0)
    {
        return false;
    }

    const uint32_t now = millis();
    float sumPm1 = 0;
    float sumPm25 = 0;
    float sumPm10 = 0;
    size_t validCount = 0;

    float validPm1[CAPACITY];
    float validPm25[CAPACITY];
    float validPm10[CAPACITY];

    for (size_t i = 0; i < count; ++i)
    {
        if (now - samples[i].timestamp <= 60000)
        {
            if (validCount == 0)
            {
                summary.lowPm1 = samples[i].pm1;
                summary.highPm1 = samples[i].pm1;
                summary.lowPm25 = samples[i].pm25;
                summary.highPm25 = samples[i].pm25;
                summary.lowPm10 = samples[i].pm10;
                summary.highPm10 = samples[i].pm10;
            }
            else
            {
                summary.lowPm1 = min(summary.lowPm1, samples[i].pm1);
                summary.highPm1 = max(summary.highPm1, samples[i].pm1);
                summary.lowPm25 = min(summary.lowPm25, samples[i].pm25);
                summary.highPm25 = max(summary.highPm25, samples[i].pm25);
                summary.lowPm10 = min(summary.lowPm10, samples[i].pm10);
                summary.highPm10 = max(summary.highPm10, samples[i].pm10);
            }

            sumPm1 += samples[i].pm1;
            sumPm25 += samples[i].pm25;
            sumPm10 += samples[i].pm10;
            validPm1[validCount] = samples[i].pm1;
            validPm25[validCount] = samples[i].pm25;
            validPm10[validCount] = samples[i].pm10;
            validCount++;
        }
    }

    if (validCount == 0)
    {
        return false;
    }

    summary.averagePm1 = sumPm1 / (float)validCount;
    summary.averagePm25 = sumPm25 / (float)validCount;
    summary.averagePm10 = sumPm10 / (float)validCount;
    summary.medianPm1 = median(validPm1, validCount);
    summary.medianPm25 = median(validPm25, validCount);
    summary.medianPm10 = median(validPm10, validCount);
    return true;
}

void AirQualityStats::clear()
{
    head = 0;
    count = 0;
}

size_t AirQualityStats::getCount() const
{
    return count;
}