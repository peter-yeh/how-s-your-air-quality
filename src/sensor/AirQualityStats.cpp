#include "AirQualityStats.h"
#include <algorithm>

namespace
{
    constexpr size_t MEDIAN_SAMPLE_COUNT = 5;

    float median(const float *values, size_t count)
    {
        float sortedValues[MEDIAN_SAMPLE_COUNT];
        std::copy(values, values + count, sortedValues);
        std::sort(sortedValues, sortedValues + count);
        if (count % 2 == 1)
        {
            return sortedValues[count / 2];
        }
        return (sortedValues[count / 2 - 1] + sortedValues[count / 2]) / 2.0f;
    }
}

void AirQualityStats::MedianEstimator::add(float value)
{
    if (initialCount < MARKER_COUNT)
    {
        initialValues[initialCount++] = value;
        if (initialCount == MARKER_COUNT)
        {
            std::sort(initialValues, initialValues + MARKER_COUNT);
            for (uint8_t markerIndex = 0; markerIndex < MARKER_COUNT; ++markerIndex)
            {
                markerHeights[markerIndex] = initialValues[markerIndex];
                markerPositions[markerIndex] = markerIndex + 1;
                desiredPositions[markerIndex] = markerIndex + 1;
            }
            positionIncrements[0] = 0;
            positionIncrements[1] = 0.25f;
            positionIncrements[2] = 0.5f;
            positionIncrements[3] = 0.75f;
            positionIncrements[4] = 1;
        }
        return;
    }

    int intervalIndex = 0;
    if (value < markerHeights[0])
    {
        markerHeights[0] = value;
    }
    else if (value >= markerHeights[4])
    {
        intervalIndex = 3;
        markerHeights[4] = value;
    }
    else
    {
        while (intervalIndex < 3 && value >= markerHeights[intervalIndex + 1])
        {
            ++intervalIndex;
        }
    }

    for (int markerIndex = intervalIndex + 1; markerIndex < MARKER_COUNT; ++markerIndex)
    {
        ++markerPositions[markerIndex];
    }
    for (int markerIndex = 0; markerIndex < MARKER_COUNT; ++markerIndex)
    {
        desiredPositions[markerIndex] += positionIncrements[markerIndex];
    }

    for (int markerIndex = 1; markerIndex < MARKER_COUNT - 1; ++markerIndex)
    {
        const float desiredDelta = desiredPositions[markerIndex] - markerPositions[markerIndex];
        const int direction = desiredDelta >= 0 ? 1 : -1;
        const bool canMove = (direction > 0 && markerPositions[markerIndex + 1] - markerPositions[markerIndex] > 1) ||
                             (direction < 0 && markerPositions[markerIndex - 1] - markerPositions[markerIndex] < -1);
        if (!canMove || (direction > 0 && desiredDelta < 1) || (direction < 0 && desiredDelta > -1))
        {
            continue;
        }

        const float predictedHeight = markerHeights[markerIndex] +
                                      direction / (markerPositions[markerIndex + 1] - markerPositions[markerIndex - 1]) *
                                          ((markerPositions[markerIndex] - markerPositions[markerIndex - 1] + direction) *
                                               (markerHeights[markerIndex + 1] - markerHeights[markerIndex]) /
                                               (markerPositions[markerIndex + 1] - markerPositions[markerIndex]) +
                                           (markerPositions[markerIndex + 1] - markerPositions[markerIndex] - direction) *
                                               (markerHeights[markerIndex] - markerHeights[markerIndex - 1]) /
                                               (markerPositions[markerIndex] - markerPositions[markerIndex - 1]));

        if (predictedHeight > markerHeights[markerIndex - 1] && predictedHeight < markerHeights[markerIndex + 1])
        {
            markerHeights[markerIndex] = predictedHeight;
        }
        else
        {
            markerHeights[markerIndex] += direction *
                                          (markerHeights[markerIndex + direction] - markerHeights[markerIndex]) /
                                          (markerPositions[markerIndex + direction] - markerPositions[markerIndex]);
        }
        markerPositions[markerIndex] += direction;
    }
}

float AirQualityStats::MedianEstimator::get() const
{
    if (initialCount == 0)
    {
        return 0;
    }
    if (initialCount < MARKER_COUNT)
    {
        return median(initialValues, initialCount);
    }
    return markerHeights[2];
}

void AirQualityStats::MedianEstimator::clear()
{
    initialCount = 0;
}

void AirQualityStats::addSample(float pm1, float pm25, float pm10)
{
    if (count == 0)
    {
        lowPm1 = pm1;
        highPm1 = pm1;
        lowPm25 = pm25;
        highPm25 = pm25;
        lowPm10 = pm10;
        highPm10 = pm10;
    }
    else
    {
        lowPm1 = std::min(lowPm1, pm1);
        highPm1 = std::max(highPm1, pm1);
        lowPm25 = std::min(lowPm25, pm25);
        highPm25 = std::max(highPm25, pm25);
        lowPm10 = std::min(lowPm10, pm10);
        highPm10 = std::max(highPm10, pm10);
    }

    sumPm1 += pm1;
    sumPm25 += pm25;
    sumPm10 += pm10;
    medianPm1.add(pm1);
    medianPm25.add(pm25);
    medianPm10.add(pm10);
    ++count;
}

bool AirQualityStats::getSummary(AirQualitySummary &summary) const
{
    if (count == 0)
    {
        return false;
    }

    summary.lowPm1 = lowPm1;
    summary.highPm1 = highPm1;
    summary.averagePm1 = static_cast<float>(sumPm1 / static_cast<double>(count));
    summary.medianPm1 = medianPm1.get();
    summary.lowPm25 = lowPm25;
    summary.highPm25 = highPm25;
    summary.averagePm25 = static_cast<float>(sumPm25 / static_cast<double>(count));
    summary.medianPm25 = medianPm25.get();
    summary.lowPm10 = lowPm10;
    summary.highPm10 = highPm10;
    summary.averagePm10 = static_cast<float>(sumPm10 / static_cast<double>(count));
    summary.medianPm10 = medianPm10.get();
    return true;
}

void AirQualityStats::clear()
{
    count = 0;
    sumPm1 = 0;
    sumPm25 = 0;
    sumPm10 = 0;
    medianPm1.clear();
    medianPm25.clear();
    medianPm10.clear();
}

size_t AirQualityStats::getCount() const
{
    return count;
}