/**
 ******************************************************************************
 * @file
 * @author  Samuel Almeida Rocha
 * @version
 * @date
 * @ingroup
 * @brief   
 *
 ******************************************************************************
 */

#ifndef DEVICES_ESP_AT_ESP_AT_HPP_
#define DEVICES_ESP_AT_ESP_AT_HPP_

// Helios includes
#include <hel_fsm>
#include <hel_pit>
#include <hel_string>

// Drivers includes
#include "uart/uart.hpp"
#include "gpio/gpio.hpp"

// FreeRTOS includes
#include "freertos/task.hpp"
#include "freertos/queue.hpp"

#include "esp_basic_cmds.hpp"
#include "esp_wifi_cmds.hpp"

namespace hel
{

constexpr size_t kEsp32StackSize = 256;

class EspAt :
  public StaticTask<kEsp32StackSize>, Uart
{
public:

  // Constructor takes a UART handle for communication and a GPIO reference for resetting the ESP32.
  EspAt(UART_handle& handle, Gpio& resetPin);

  enum class Event
  {
    Ready,
    WiFiConnected,
    Time,
    MqttConnected,
    MqttRecv,
    ListAp
  };

  // Accessor for the temporary TX buffer.
  String& txBuffer() noexcept { return m_temp_string; }

  // Execute an AT command and wait for the expected response.
  ReturnCode execCommand(const char* expected_response, uint32_t timeout_ms =
      kDefaultTimeoutMs) noexcept;


private: // Constants
   static constexpr std::size_t kFsmTransitionsCapacity = 10U;

   static constexpr uint32_t kDefaultTimeoutMs = 2000U;

   static constexpr size_t kBuffSize = 256U;

   static constexpr size_t kEspAtUrcListSize = 6U;
   static constexpr size_t kEspAtUrcQueueSize = 2U;

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

   /** @brief Associates a URC search key with its dispatch callback. */
   struct Esp32AtEvent
   {
     const char *key;                              ///< The event string key
     alignas(void*) void (EspAt::*callback)(String &);         ///< Pointer to the member function
   };

private: // Members
  hel::FSM<EspAt, kFsmTransitionsCapacity> m_fsm;
  Gpio &m_resetPin;

  Esp32AtEvent m_urc_entries[kEspAtUrcListSize]; /*!< Registered URC key/callback pairs. */
  std::size_t m_urc_count; /*!< Number of registered URC entries. */

  Esp32BasicCmds m_basic_cmds { *this };

  // DMA receive buffer and current RX frame.
  StringData<kBuffSize>m_rx_string;

  // Temporary buffer for building AT command strings before transmission.
  StringData<kBuffSize>m_temp_string;

  // Mailbox for URC messages from ISR to task context.
  StaticQueue<StringData<kBuffSize>, kEspAtUrcQueueSize> m_urc_buffer;

  // FSM states
  FSMState<EspAt>m_state_init;
  FSMState<EspAt>m_state_idle;
  FSMState<EspAt>m_state_busy;
  FSMState<EspAt>m_state_error;

  //
  Pit m_cmd_timeout; /*!< Timeout for command responses. */
  volatile bool m_waiting_for_response = false; /*!< Set by waitForResponse, cleared by ISR. */

  bool m_ready; /*!< Indicates if the ESP32 is ready after reset. */
  bool m_connected; /*!< Tracks Wi-Fi connection status. */

private: // Methods

  // Task entry point. Called by FreeRTOS when the task is scheduled.
  void run() noexcept override;

  void reset() noexcept;

  // Process any pending URC messages in the mailbox.
  void processURC() noexcept;

  // UART event handler registered as the UartCallback.
  void handleEvent(UartEvent event, uint16_t count) noexcept;

  // Wait for a specific response string to be received, with a timeout.
  ReturnCode waitForResponse(const char* expected_response, uint32_t timeout_ms) noexcept;

  // Event Rady: Triggered when the ESP32 signals it is ready after reset.
  void onReady(String& msg);

  // Event Connect: Triggered on Wi-Fi connection status changes (e.g., "WIFI CONNECTED").
  void onConnected(String& msg);

  // Event Time: Triggered when the ESP32 reports the current time (e.g., "TIME: ...").
  void onTimeUpdated(String& msg);

  // Event MqttConnected: Triggered when the ESP32 reports MQTT connection status (e.g., "MQTT CONNECTED").
  void onMqttConn(String& msg);

  // Event MqttRecv: Triggered when the ESP32 receives an MQTT message (e.g., "MQTT RECV: ...").
  void onMqttRecvEvent(String& msg);

  // Event ListAp: Triggered when the ESP32 reports available Wi-Fi networks (e.g., "AP: ...").
  void onListAp(String& msg);

  // FSM state handlers

  //State Init: Initial state after reset, waiting for "ready" event.
  void enterStateInit();
  void onStateInit();

  //State Idle: Normal operation state, waiting for commands or events.
  void onStateIdle();

  //State Busy: State when a command is being executed, waiting for response.
  void enterStateBusy();
  void onStateBusy();
  void exitStateBusy();

  //State Error: State entered on error conditions, may attempt recovery or wait for reset.
  void onStateError();

};

} /* namespace dev */

#endif /* DEVICES_ESP_AT_ESP_AT_HPP_ */
