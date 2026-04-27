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

#include "esp_at_commons.hpp"
#include "esp_basic_cmds.hpp"
#include "esp_wifi_cmds.hpp"

namespace hel
{

class EspAt :
  public StaticTask<ESP::kEsp32StackSize>, Uart
{
public:

  // Constructor takes a UART handle for communication and a GPIO reference for resetting the ESP32.
  EspAt(UART_handle& handle, Gpio& resetPin);

  ReturnCode connectWiFi(String& ssid, String& password) noexcept;



  enum class Event
  {
    Ready,
    WiFiConnected,
    Time,
    MqttConnected,
    MqttRecv,
    ListAp
  };

  using Esp32Callback = Callback<void(Event, uint16_t)>;

  // Accessor for the temporary TX buffer.
  String& txBuffer() noexcept
  {
    m_tx_string.clear();
    return m_tx_string;
  }

  String& rxBuffer() noexcept
  {
    return m_rx_string;
  }

  // Execute an AT command and wait for the expected response.
  ReturnCode execCommand(const char* expected_response, uint32_t timeout_ms =
      ESP::kDefaultTimeoutMs) noexcept;


private: // Types

  struct Command
  {
    ESP::CommandType                    type;
    ESP::RawStringBuffer<ESP::kBuffSize> payload;
  };

   /** @brief Associates a URC search key with its dispatch callback. */
   struct Esp32AtEvent
   {
     const char *key;                              ///< The event string key
     alignas(void*) void (EspAt::*callback)(String &);         ///< Pointer to the member function
   };

private: // Members

  hel::FSM<EspAt, ESP::kFsmTransitionsCapacity> m_fsm;
  Gpio &m_resetPin;

  Esp32AtEvent m_urc_entries[ESP::kEspAtUrcListSize]; /*!< Registered URC key/callback pairs. */
  std::size_t m_urc_count; /*!< Number of registered URC entries. */

  Esp32BasicCmds m_basic_cmds { *this };

  // DMA receive buffer and current RX frame.
  StringData<ESP::kBuffSize>m_rx_string;
  StringData<ESP::kBuffSize>m_tx_string;

  // Temporary buffer for building AT command strings before transmission.
  Command m_temp_command;

  // Mailbox for URC messages from ISR to task context.
  StaticQueue<ESP::RawStringBuffer<ESP::kBuffSize>, ESP::kEspAtUrcQueueSize> m_urc_buffer;

  // Queue for pending commands to be processed by the FSM.
  StaticQueue<Command, ESP::kEspCmdQueueSize> m_command_queue;


  // FSM states
  FSMState<EspAt>m_state_init;
  FSMState<EspAt>m_state_idle;
  FSMState<EspAt>m_state_busy;
  FSMState<EspAt>m_state_error;

  // Timer for command timeouts, used in waitForResponse and state transitions.
  Pit m_cmd_timeout;

  // Flag indicating whether waitForResponse.
  volatile bool m_waiting_for_response = false;
  // Flags for tracking ESP32 status based on URC events.
  bool m_ready;

  // Flag indicating Wi-Fi connection status.
  bool m_wifi_connected;

  StringData<10> m_module_version;

private: // Inherited methods

  // Task entry point. Called by FreeRTOS when the task is scheduled.
  void run() noexcept override;

  // UART event handler registered as the UartCallback.
  void handleEvent(UartEvent event, uint16_t count) noexcept;

private: // Internal methods

  void reset() noexcept;

  // Process any pending URC messages in the mailbox.
  void processURC() noexcept;

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
