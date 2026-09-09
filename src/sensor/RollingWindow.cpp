#include "RollingWindow.h"
#include <Arduino.h>

void RollingWindow::addSample(float pm1, float pm25, float pm10)
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

bool RollingWindow::getAverage(float &avgPm1, float &avgPm25, float &avgPm10) const
{
    if (count == 0)
    {
        return false;
    }

    const uint32_t now = millis();
    float sum1 = 0;
    float sum25 = 0;
    float sum10 = 0;
    size_t validCount = 0;

    for (size_t i = 0; i < count; ++i)
    {
        // Include samples collected within the 1-minute window
        if (now - samples[i].timestamp <= 61000)
        {
            sum1 += samples[i].pm1;
            sum25 += samples[i].pm25;
            sum10 += samples[i].pm10;
            validCount++;
        }
    }

    if (validCount == 0)
    {
        return false;
    }

    avgPm1 = sum1 / (float)validCount;
    avgPm25 = sum25 / (float)validCount;
    avgPm10 = sum10 / (float)validCount;
    return true;
}

void RollingWindow::clear()
{
    head = 0;
    count = 0;
}

size_t RollingWindow::getCount() const
{
    return count;
}
