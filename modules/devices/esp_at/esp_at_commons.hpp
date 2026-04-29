/**
 ******************************************************************************
 * @file    esp_ar_commons.hpp
 * @author  Samuel Almeida Rocha
 * @version 0.0.1
 * @date    Apr 21, 2026
 * @brief   
 *
 ******************************************************************************
 */
#ifndef DEVICES_ESP_AT_COMMONS_HPP_
#define DEVICES_ESP_AT_COMMONS_HPP_

#include <cstdint>
#include <cstddef>
#include <cstring>

namespace hel
{

namespace ESP
{

constexpr size_t kEsp32StackSize = 512;

constexpr const char *kExpectedOk = "OK\r\n";

// Timing constants for command execution and FSM timeouts.
static constexpr uint32_t kSmallDelayMs = 50U;
static constexpr uint32_t kDefaultTimeoutMs = 2000U;
static constexpr uint32_t kinitTimeoutMs = 5000U;
static constexpr uint32_t kListApTimeout = 15000U;

static constexpr size_t kBuffSize = 512;
static constexpr size_t kEspAtUrcListSize = 6U;
static constexpr size_t kEspAtUrcQueueSize = 3U;

/**
 * @brief   Wi-Fi station connection state reported by AT+CWSTATE.
 */
enum class WiFiState : uint8_t
{
  NotStarted     = 0U, /*!< Wi-Fi not initialized (AT+CWINIT=0)           */
  ConnectedNoIP  = 1U, /*!< Connected to AP but IP not yet obtained       */
  ConnectedWithIP = 2U,/*!< Connected to AP and IP obtained               */
  Connecting     = 3U, /*!< Connection in progress (AT+CWJAP executing)   */
  Disconnected   = 4U  /*!< Disconnected from AP                          */
};

enum class ApMode : uint8_t
{
  Off          = 0U, /*!< Wi-Fi disabled                                   */
  Station      = 1U, /*!< Station mode (STA)                               */
  SoftAP       = 2U, /*!< SoftAP mode                                      */
  StationSoftAP = 3U /*!< Concurrent Station + SoftAP mode                 */
};



/**
 * @brief   UTC offset value for SNTP configuration.
 * @details The ESP32 AT firmware accepts the timezone as an integer that
 *          encodes the UTC offset in one of two ways:
 *          - Whole-hour offsets: pass the number of hours directly
 *            (e.g. @c -5 for UTC-05:00, @c 8 for UTC+08:00).
 *          - Sub-hour offsets: pass hours * 100 + minutes
 *            (e.g. @c 530 for UTC+05:30, @c 1245 for UTC+12:45).
 */
using TimeZone = int16_t;

} // namespace ESP

} // namespace hel

#endif /* DEVICES_ESP_AT_COMMONS_HPP_ */
