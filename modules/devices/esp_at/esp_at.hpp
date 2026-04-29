/**
 ******************************************************************************
 * @file    esp_at.hpp
 * @author  Samuel Almeida Rocha
 * @version 0.1.0
 * @date    2026-04-27
 * @ingroup HELIOS_DEV_ESP_AT
 * @brief   ESP32 AT-command driver — Wi-Fi, TCP/IP and MQTT.
 *
 * @details
 *   - Drives an ESP32 module via UART using the official ESP-AT firmware.
 *   - Implements a FreeRTOS task that processes unsolicited result codes (URCs)
 *     and exposes synchronous command methods to the caller.
 *   - All blocking methods busy-wait internally; they must be called from task
 *     context only — never from ISR.
 *   - Commands serialize through a single TX buffer (@ref m_tx_string); they
 *     are therefore NOT re-entrant.  Call from a single owner task or guard
 *     with a mutex.
 *   - URC dispatch uses a FreeRTOS queue that is written from ISR and drained
 *     from task context.
 *
 * @note    Requires UART DMA-to-idle reception already configured by the
 *          caller before @ref EspAt is constructed.
 *
 ******************************************************************************
 */

#ifndef DEVICES_ESP_AT_ESP_AT_HPP_
#define DEVICES_ESP_AT_ESP_AT_HPP_

// Helios includes
#include <hel_pit>
#include <hel_string>
#include <hel_span>
#include <hel_calendar>

// Drivers includes
#include "uart/uart.hpp"
#include "gpio/gpio.hpp"

// FreeRTOS includes
#include "freertos/task.hpp"
#include "freertos/queue.hpp"

#include "esp_at_commons.hpp"

namespace hel
{

/**
 * @brief   ESP32 AT-command driver.
 * @details
 *   - Inherits @ref StaticTask to run its own FreeRTOS task.
 *   - Inherits @ref Uart for UART DMA send / receive.
 *   - Exposes synchronous Wi-Fi, TCP/IP and MQTT command methods.
 * @ingroup HELIOS_DEV_ESP_AT
 */
class EspAt :
  public StaticTask<ESP::kEsp32StackSize>, Uart
{
public:

  // Construct bound to @p handle for UART I/O and @p resetPin for hard reset.
  EspAt(UART_handle& handle, Gpio& resetPin);
  /**
   * @brief   Application-level events fired by URC handlers.
   */
  enum class Event
  {
    Ready,              /*!< Module has reset and is ready to receive commands    */
    WiFiConnection,      /*!< Station connected to an AP ("WIFI CONNECTED")       */
    TimeUpdated,               /*!< SNTP time synchronised ("TIME_UPDATED")             */
    MqttConnected,      /*!< MQTT broker connection state changed                */
    MqttRecv,           /*!< Inbound MQTT message received on a subscribed topic */
    ListAp              /*!< One AP entry returned by AT+CWLAP scan              */
  };

  // Type alias for application-level event callbacks.
  using EventCallback = Callback<void(Event, String&)>;

  enum class ApEncryption : uint8_t
  {
    Open = 0U, /*!< No encryption (open network)                     */
    WEP  = 1U, /*!< WEP encryption                                  */
    WPA_PSK = 2U, /*!< WPA-PSK encryption                             */
    WPA2_PSK = 3U, /*!< WPA2-PSK encryption                            */
    WPA_WPA2_PSK = 4U, /*!< WPA/WPA2 mixed-mode PSK encryption             */
    WPA2_ENTERPRISE = 5U, /*!< WPA2 Enterprise encryption                    */
    WPA3_PSK = 6U, /*!< WPA3-PSK encryption                             */
    WPA2_WPA3_PSK = 7U, /*!< WPA2/WPA3 mixed-mode PSK encryption            */
    WAPI_PSK = 8U, /*!< WAPI-PSK encryption                            */
    OWE = 9U, /*!< OWE (Opportunistic Wireless Encryption)         */
    WPA3_ENT_192 = 10U, /*!< WPA3 Enterprise with 192-bit security         */
    WPA3_EXT_PSK = 11U, /*!< WPA3 with external PSK (e.g. from DPP)        */
    WPA3_EXT_PSK_MIXED_MODE = 12U, /*!< Mixed-mode WPA3 with external PSK        */
    DPP = 13U, /*!< DPP (Device Provisioning Protocol)              */
    WPA3_ENTERPRISE = 14U, /*!< WPA3 Enterprise with EAP authentication       */
    WPA2_WPA3_ENTERPRISE = 15U /*!< Mixed-mode WPA2/WPA3 Enterprise with EAP   */
  };

  struct ApInfo
  {
    char ssid[33] = { }; // 32 chars + null terminator
    uint8_t rssi;           /*!< Signal strength 0 (weak) to 100 (strong) */
    ApEncryption encryption; /*!< Encryption type (see @ref ApEncryption) */
  };


  // ---------------------------------------------------------------------------
  // Wi-Fi management commands
  // ---------------------------------------------------------------------------

  // Get Wifi driver status.
  [[nodiscard]] ReturnCode getWiFiStatus(bool& enabled) noexcept;

  // Initialize (enable=true) or deinitialize (enable=false) the Wi-Fi driver.
  [[nodiscard]] ReturnCode enableWiFi(bool enable) noexcept;

  // Set the Wi-Fi operating mode (Station, SoftAP, or both).
  [[nodiscard]] ReturnCode setWiFiMode(ESP::ApMode mode) noexcept;

  // Connect the ESP32 station to an AP; waits up to 15 s for "OK".
  [[nodiscard]] ReturnCode connectWiFi(String ssid, String password) noexcept;

  // Disconnect the ESP32 station from the AP.
  [[nodiscard]] ReturnCode disconnectWiFi() noexcept;

  // Query the current Wi-Fi connection state via AT+CWSTATE.
  [[nodiscard]] ReturnCode wiFiStatus(ESP::WiFiState& state) noexcept;

  // Query SSID, BSSID and RSSI of the connected AP via AT+CWJAP?.
  [[nodiscard]] ReturnCode getApInfo(
    String& ssid, String& bssid, int& rssi, bool& is_secure) noexcept;

  // Obtain the IP address of a station connected to the ESP32 SoftAP.
  [[nodiscard]] ReturnCode getIpAddress(String& ip) noexcept;

  // Start an AP scan (AT+CWLAP); results arrive as ListAp URC events.
  [[nodiscard]] ReturnCode listAp(Span<EspAt::ApInfo> ap_array, uint8_t& count) noexcept;

  // Read the ESP32 internal temperature sensor.
  [[nodiscard]] ReturnCode getTemperature(float& temperature) noexcept;

  // ---------------------------------------------------------------------------
  // TCP / IP commands
  // ---------------------------------------------------------------------------

  // Configure the SNTP time zone and server (AT+CIPSNTPCFG).
  [[nodiscard]] ReturnCode setTimeZoneAndSNTP(
    ESP::TimeZone tz, String& sntp_server) noexcept;

  // Ping a remote host and return on the first reply or timeout.
  [[nodiscard]] ReturnCode ping(String& host, uint32_t timeout_ms) noexcept;

  // ---------------------------------------------------------------------------
  // MQTT commands
  // ---------------------------------------------------------------------------

  // Set MQTT scheme and client ID via AT+MQTTUSERCFG (username/password empty).
  [[nodiscard]] ReturnCode setMqttUserConfig(
    int scheme, const char* client_id) noexcept;

  // Set a long MQTT client ID via AT+MQTTLONGCLIENTID (two-phase protocol).
  [[nodiscard]] ReturnCode setMqttClientId(String& client_id) noexcept;

  // Set a long MQTT username via AT+MQTTLONGUSERNAME (two-phase protocol).
  [[nodiscard]] ReturnCode setMqttUsername(String& username) noexcept;

  // Set a long MQTT password via AT+MQTTLONGPASSWORD (two-phase protocol).
  [[nodiscard]] ReturnCode setMqttPassword(String& password) noexcept;

  // Connect to an MQTT broker via AT+MQTTCONN.
  [[nodiscard]] ReturnCode mqttConnect(
    String& endpoint, uint16_t port) noexcept;

  // Publish an MQTT message as a quoted string via AT+MQTTPUB.
  [[nodiscard]] ReturnCode mqttPublish(
    String& topic, String& msg) noexcept;

  // Publish a raw binary MQTT payload via AT+MQTTPUBRAW (two-phase protocol).
  [[nodiscard]] ReturnCode mqttPublishRaw(
    String& topic, uint8_t* msg, size_t len) noexcept;

  // Subscribe to an MQTT topic via AT+MQTTSUB (QoS 0).
  [[nodiscard]] ReturnCode mqttSubscribe(String& topic) noexcept;

  // Unsubscribe from an MQTT topic via AT+MQTTUNSUB.
  [[nodiscard]] ReturnCode mqttUnsubscribe(String& topic) noexcept;

  // Close the MQTT connection and free resources via AT+MQTTCLEAN.
  [[nodiscard]] ReturnCode mqttCLose() noexcept;

  void setEventCallback(EventCallback callback) noexcept
  {
    m_event_callback = callback;
  }


private: // Types

private: // Members

  Gpio& m_resetPin;

  // DMA receive buffer and current RX frame.
  char m_rx_buffer[ESP::kBuffSize];
  String m_rx_string { m_rx_buffer };

  // Buffer for constructing outgoing AT command strings.
  char m_tx_buffer[ESP::kBuffSize];
  String m_tx_string { m_tx_buffer };

  // Scratch buffer for ISR-to-task URC copies.
  char m_temp_buffer[ESP::kBuffSize];

  // Mailbox for URC messages from ISR to task context.
  StaticQueue<char[ESP::kBuffSize], ESP::kEspAtUrcQueueSize> m_urc_buffer;

  // Timer for command timeouts and FSM state transitions.
  Pit m_cmd_timeout;

  EventCallback m_event_callback;

  // Set true by waitForResponse; cleared from ISR when a frame arrives.
  volatile bool m_waiting_for_response = false;

  bool m_ready;         ///< True after "ready" URC received.
  bool m_initialized;   ///< True after successful stateInit sequence.
  bool m_wifi_connected; ///< True when "WIFI CONNECTED" URC received.
  bool m_mqtt_connected; ///< True when "+MQTTCONNECTED:0" URC received.

private: // Inherited — task entry point

  // FreeRTOS task body.
  void run() noexcept override;

  // UART event callback (called from ISR context).
  void handleEvent(UartEvent event, uint16_t count) noexcept;

private: // Internal command helpers

  // Hard-reset the ESP32 and clear all state flags.
  void reset() noexcept;

  // Drain the URC mailbox and dispatch each pending message.
  void processURC() noexcept;

  /**
   * @brief   Append "\r\n", transmit @ref m_tx_string and wait for one frame.
   * @details Single-frame variant: returns as soon as the first non-CRLF
   *          frame arrives.  Use @ref execCommandMulti for commands that
   *          produce intermediate frames before the final "OK".
   */
  [[nodiscard]] ReturnCode execCommand(
    const char* expected_response = nullptr,
    uint32_t    timeout_ms = ESP::kDefaultTimeoutMs) noexcept;

  /**
   * @brief   Like @ref execCommand but keeps waiting across intermediate frames.
   * @details Continues re-arming the wait loop when a frame is received that
   *          does not match @p expected_response and does not contain "ERROR".
   *          Use this for AT+CWJAP, AT+MQTTCONN, AT+CWLAP, etc.
   */
  [[nodiscard]] ReturnCode execCommandMulti(
    const char* expected_response,
    uint32_t    timeout_ms = ESP::kDefaultTimeoutMs) noexcept;


  /**
   * @brief   Send raw bytes via @ref m_tx_string and wait for "OK".
   * @details Phase-2 helper for the AT LONG-data protocol.  Caller must
   *          already have sent the header command and received the ">" prompt
   *          via @ref execCommand before invoking this method.
   * @note    Binary payloads that contain null bytes may be truncated if the
   *          underlying @ref String::append stops at '\0'.
   */
  [[nodiscard]] ReturnCode execLongDataCommand(
    const char* data,
    std::size_t len,
    uint32_t    timeout_ms = ESP::kDefaultTimeoutMs) noexcept;

  // Busy-wait for a single matching frame; called by execCommand.
  [[nodiscard]] ReturnCode waitForResponse(
    const char* expected_response, uint32_t timeout_ms) noexcept;

  // Busy-wait across multiple frames; called by execCommandMulti.
  [[nodiscard]] ReturnCode waitForResponseMulti(
    const char* expected_response, uint32_t timeout_ms) noexcept;

private: // Configuration helpers

  // Enable (true) or disable (false) command echo (ATE1 / ATE0).
  [[nodiscard]] ReturnCode setEcho(bool enable) noexcept;

private: // URC event handlers

  // "ready" — ESP32 has completed its reset sequence.
  void onReady(String& msg) noexcept;

  // "WIFI CONNECTED" / "WIFI DISCONNECTED" — station link state changed.
  void onConnected(String& msg) noexcept;

  // "TIME_UPDATED" / "WIFI GOT IP" — IP or SNTP time is now available.
  void onTimeUpdated(String& msg) noexcept;

  // "+MQTTCONNECTED" / "+MQTTDISCONNECTED" — broker connection state changed.
  void onMqttConn(String& msg) noexcept;

  // "+MQTTSUBRECV" — inbound MQTT message on a subscribed topic.
  void onMqttRecvEvent(String& msg) noexcept;

  // "+CWLAP" — one AP entry from an ongoing AT+CWLAP scan.
  void onListAp(String& msg) noexcept;

private: // FSM state handlers

  // Initial state: reset the module and wait for "ready".
  void stateInit() noexcept;

  // Idle state: normal operation, process commands and URCs.
  void stateIdle() noexcept;
};

} // namespace hel

#endif /* DEVICES_ESP_AT_ESP_AT_HPP_ */
