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
static constexpr uint32_t kDefaultTimeoutMs = 2000U;
static constexpr uint32_t kinitTimeoutMs = 5000U;

static constexpr std::size_t kFsmTransitionsCapacity = 10U;
static constexpr size_t kBuffSize = 256U;
static constexpr size_t kEspAtUrcListSize = 6U;
static constexpr size_t kEspAtUrcQueueSize = 3U;
static constexpr size_t kEspCmdQueueSize = 3U;

enum // FSM trigger events
{
  TriggerInit,
  TriggerReady,
  TriggerIdle,
  TriggerCommand,
  TriggerBusy,
  TriggerSuccess,
  TriggerError
};

/**
 * @brief POD buffer for safe RTOS queue storage of string data.
 * @details FreeRTOS queues use memcpy internally, which breaks any object
 *          that contains a self-referencing pointer (like StringData). This
 *          struct stores only raw bytes and a count so it is safe to memcpy.
 */
template<std::size_t N>
struct RawStringBuffer
{
  char        data[N];
  std::size_t count;
};

enum class CommandType
{
  Reset,
  ConnectWiFi,
  DisconnectWiFi,
  GetTime,
  ConnectMqtt,
  DisconnectMqtt,
  PublishMqtt,
  SubscribeMqtt,
  UnsubscribeMqtt,
  ListAp
};


} // namespace esp_at

} /* namespace dev */

#endif /* DEVICES_ESP_AT_COMMONS_HPP_ */
