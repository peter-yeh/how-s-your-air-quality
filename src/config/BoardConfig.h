#pragma once

#include <stdint.h>

// ============ DISPLAY (TFT/ST7789) ============
namespace DisplayConfig
{
    constexpr uint8_t TFT_CS = 15;
    constexpr uint8_t TFT_DC = 2;
    constexpr uint8_t TFT_SCK = 14;
    constexpr uint8_t TFT_MOSI = 13;
    constexpr uint8_t TFT_MISO = 12;
    constexpr uint8_t TFT_BL = 27;
    constexpr uint8_t TFT_BRIGHTNESS_DEFAULT = 1;
    constexpr int16_t BASE_GRAPH_X = 36;
    constexpr int16_t BASE_GRAPH_Y = 96;
    constexpr uint8_t SMALL_FONT_SIZE = 1;
    constexpr int16_t SUMMARY_ROW_Y[] = {40, 55, 70};
    constexpr int16_t SUMMARY_VALUE_COLUMNS[] = {66, 100, 134, 168, 202};
    constexpr uint8_t SUMMARY_VALUE_CELL_WIDTH = 34;
    constexpr uint8_t SUMMARY_VALUE_MAX_CHARACTERS = 5;
}

// ============ SENSOR (BMV080 I2C) ============
namespace SensorConfig
{
    constexpr uint8_t PIN_SDA = 32;
    constexpr uint8_t PIN_SCL = 25;
    constexpr uint32_t I2C_FREQUENCY = 100000;
    constexpr uint8_t I2C_ADDR_DEFAULT = 0x57;
    constexpr uint8_t MAX_INIT_RETRIES = 5;
    constexpr uint32_t RETRY_DELAY_MS = 2000;
    constexpr uint32_t WARMUP_DELAY_MS = 30000;        // BMV080 needs ~30sec warmup
    constexpr uint32_t SENSOR_STALE_TIMEOUT_MS = 5000; // Mark data stale if no update for 5sec
}

// ============ STORAGE (SD Card SPI) ============
namespace StorageConfig
{
    constexpr uint8_t SD_CS = 5;
    constexpr uint8_t SD_SCK = 18;
    constexpr uint8_t SD_MOSI = 23;
    constexpr uint8_t SD_MISO = 19;
    constexpr uint32_t SD_FREQUENCY = 20000000;
    constexpr size_t MAX_BLE_FILE_SIZE = 1048576; // 1MB max for BLE streaming
}

// ============ WIRELESS (WiFi & NTP) ============
namespace WirelessConfig
{
    constexpr uint32_t WIFI_TIMEOUT_MS = 20000;
    constexpr uint32_t TIME_SYNC_TIMEOUT_MS = 10000;
    constexpr uint32_t WIFI_RECONNECT_INTERVAL_MS = 30000;
}

// ============ BLE SERVER ============
namespace BleConfig
{
    constexpr int MAX_CHUNK_SIZE = 244;  // 244 + 3 (ATT) + 4 (L2CAP) = 251 bytes
    constexpr size_t TX_QUEUE_SIZE = 20; // Increased from 5 to handle multiple requests
    constexpr uint32_t TX_TASK_STACK_SIZE = 4096;
    constexpr uint32_t TX_RETRY_DELAY_MS = 200;
    constexpr uint32_t TX_RETRY_MAX_ATTEMPTS = 10;
}

// ============ LOGGER ============
namespace LoggerConfig
{
    constexpr size_t LOG_LINE_CAPACITY = 512;
    constexpr size_t LOG_BATCH_CAPACITY = 8192;
    constexpr size_t LOG_BATCH_COUNT = 3;
    constexpr size_t LOG_BATCH_FLUSH_SIZE = 6144;
    constexpr uint32_t LOG_BATCH_FLUSH_INTERVAL_MS = 30000; // 30 seconds, reduced from 60 to minimize log loss
    constexpr uint32_t LOG_CRITICAL_FLUSH_MS = 5000;        // Flush critical logs faster
}

// ============ SETTINGS ============
namespace SettingsConfig
{
    constexpr uint8_t DEFAULT_BRIGHTNESS = 128;
    constexpr uint8_t MAX_BRIGHTNESS = 255;
    constexpr uint8_t DEFAULT_GRAPH_MODE = 0;
    constexpr uint8_t MAX_GRAPH_MODE = 2;
}

// ============ APPLICATION TIMING ============
namespace TimingConfig
{
    constexpr uint32_t DISPLAY_UPDATE_INTERVAL_MS = 1000;
    constexpr uint32_t SENSOR_READ_INTERVAL_MS = 1000;
    constexpr uint32_t MINUTE_AGGREGATION_INTERVAL_MS = 60000;
    constexpr uint32_t BURN_IN_SHIFT_INTERVAL_MS = 60000;
    constexpr uint32_t DAILY_FILE_CHECK_INTERVAL_MS = 1000;
    constexpr uint32_t TIME_VALIDATION_MIN_YEAR = 2020; // System clock should be after 2020
}
