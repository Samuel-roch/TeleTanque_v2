/**
 ******************************************************************************
 * @file    esp_at.cpp
 * @author  Samuel Almeida Rocha
 * @version 0.1.0
 * @date    2026-04-27
 * @ingroup HELIOS_DEV_ESP_AT
 * @brief   Implementation of the ESP32 AT-command driver.
 *
 ******************************************************************************
 */

#include "esp_at.hpp"
#include <hel_stringlist>

namespace hel
{

using namespace ESP;

// ============================================================================
// Construction / task entry
// ============================================================================

EspAt::EspAt(UART_handle& handle, Gpio& resetPin)
    :
      StaticTask("Esp32", TaskPriority::Normal), Uart(handle),
      m_resetPin(resetPin)
{
  m_ready = false;
  m_initialized = false;
  m_wifi_connected = false;
  m_mqtt_connected = false;
}

void EspAt::run() noexcept
{
  (void) TaskBase::notifyTake();

  for ( ;; )
  {
    processURC();

    if (!m_ready || !m_initialized)
    {
      stateInit();
    }
    else
    {
      stateIdle();
    }

    hel::sleep(10);
  }
}

// ============================================================================
// Wi-Fi management commands
// ============================================================================

ReturnCode EspAt::getWiFiStatus(bool& enabled) noexcept
{
  m_tx_string << "AT+CWINIT?";
  const ReturnCode rc = execCommand("+CWINIT:");
  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }

  // Response: +CWINIT:<n>
  const std::size_t colon = m_rx_string.find(':');
  if (colon == String::npos)
  {
    return ReturnCode::ErrorInvalidState;
  }

  const char digit = m_rx_string.data()[colon + 1U];
  if (digit < '0' || digit > '1')
  {
    return ReturnCode::ErrorInvalidState;
  }

  enabled = (digit == '1');
  return ReturnCode::AnsweredRequest;
}

/**
 * @brief   Initialize or deinitialize the ESP32 Wi-Fi driver (AT+CWINIT).
 * @details Sends @c AT+CWINIT=1 to start the driver or @c AT+CWINIT=0 to
 *          stop it and release resources.  Deinitializing while connected
 *          will drop the link.
 *
 * @param[in] enable  @c true to enable the Wi-Fi driver; @c false to disable.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Command accepted; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : No response within the default timeout.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 *
 * @note    Must be called from task context; not ISR-safe.
 */
ReturnCode EspAt::enableWiFi(bool enable) noexcept
{
  m_tx_string.printf("AT+CWINIT=%d", enable ? 1 : 0);
  return execCommand(kExpectedOk);
}

/**
 * @brief   Set the Wi-Fi operating mode (AT+CWMODE).
 * @details
 *   - @ref ESP::ApMode::Off          → null mode (radio off)
 *   - @ref ESP::ApMode::Station      → STA only
 *   - @ref ESP::ApMode::SoftAP       → AP only
 *   - @ref ESP::ApMode::StationSoftAP → concurrent STA + AP
 *
 * @param[in] mode  Desired Wi-Fi mode.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Mode set; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : No response within the default timeout.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::setWiFiMode(ESP::ApMode mode) noexcept
{
  m_tx_string << "AT+CWMODE=" << static_cast<int>(mode);
  return execCommand(kExpectedOk);
}

/**
 * @brief   Connect the ESP32 station to a target AP (AT+CWJAP).
 * @details Sends @c AT+CWJAP="<ssid>","<password>" and waits up to 15 s for
 *          the final "OK" from the firmware.  Intermediate frames such as
 *          "WIFI CONNECTED" and "WIFI GOT IP" are transparently skipped by
 *          the multi-frame wait loop.
 *
 * @param[in] ssid      Network SSID (null-terminated; must not contain @c ").
 * @param[in] password  WPA/WPA2 passphrase (null-terminated; must not contain @c ").
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Connected; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : No "OK" within 15 s.
 *   - @ref ReturnCode::ErrorGeneral      : Firmware returned "ERROR" (wrong
 *                                          credentials or AP unreachable).
 *
 * @warning SSID and password must not contain double-quote (@c ") characters;
 *          they are injected directly into the AT command string.
 */
ReturnCode EspAt::connectWiFi(String ssid, String password) noexcept
{
  m_tx_string << "AT+CWJAP=\"" << ssid << "\",\"" << password << "\"";
  return execCommandMulti(kExpectedOk, 15000U);
}

/**
 * @brief   Disconnect the ESP32 station from the current AP (AT+CWQAP).
 *
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Disconnected; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : No response within the default timeout.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::disconnectWiFi() noexcept
{
  m_tx_string << "AT+CWQAP";
  return execCommand(kExpectedOk);
}

/**
 * @brief   Query the current Wi-Fi station connection state (AT+CWSTATE?).
 * @details Parses the leading digit from the @c +CWSTATE:<n>,"<ssid>" response
 *          and maps it to the @ref ESP::WiFiState enum.
 *
 * @param[out] state  Populated with the current connection state on success.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : @p state is valid.
 *   - @ref ReturnCode::ErrorInvalidState : Response received but could not be parsed.
 *   - @ref ReturnCode::ErrorTimeout      : No response within the default timeout.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::wiFiStatus(ESP::WiFiState& state) noexcept
{
  m_tx_string << "AT+CWSTATE?";
  const ReturnCode rc = execCommand("+CWSTATE:");
  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }

  // Response: +CWSTATE:<n>,"<ssid>"  — parse the single state digit.
  const std::size_t colon = m_rx_string.find(':');
  if (colon == String::npos)
  {
    return ReturnCode::ErrorInvalidState;
  }

  const char digit = m_rx_string.data()[colon + 1U];
  if (digit < '0' || digit > '4')
  {
    return ReturnCode::ErrorInvalidState;
  }

  state = static_cast<ESP::WiFiState>(digit - '0');
  return ReturnCode::AnsweredRequest;
}

/**
 * @brief   Query the connected AP's SSID, BSSID and RSSI (AT+CWJAP?).
 * @details Parses the @c +CWJAP:"<ssid>","<bssid>",<ch>,<rssi>,... response.
 *          The @p is_secure field is always set to @c false because the
 *          AT+CWJAP? response does not include an ECN/encryption field.
 *
 * @param[out] ssid       AP SSID string on success; unchanged on error.
 * @param[out] bssid      AP MAC address string (format "aa:bb:cc:dd:ee:ff") on success.
 * @param[out] rssi       Received signal strength in dBm on success.
 * @param[out] is_secure  Always set to @c false (ECN not available from this command).
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : All output parameters are valid.
 *   - @ref ReturnCode::ErrorInvalidState : Not connected or response parse failed.
 *   - @ref ReturnCode::ErrorTimeout      : No response within the default timeout.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::getApInfo(
  String& ssid, String& bssid, int& rssi, bool& is_secure) noexcept
{
  m_tx_string << "AT+CWJAP?";
  const ReturnCode rc = execCommand("+CWJAP:");
  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }

  // Response: +CWJAP:"<ssid>","<bssid>",<channel>,<rssi>,...
  char ssid_buf[65] = { };
  char bssid_buf[18] = { };
  int ch = 0;

  if (m_rx_string.scanf("+CWJAP:\"%64[^\"]\",\"%17[^\"]\",%d,%d", ssid_buf,
      bssid_buf, &ch, &rssi) < 4)
  {
    return ReturnCode::ErrorInvalidState;
  }

  ssid.clear();
  ssid << ssid_buf;
  bssid.clear();
  bssid << bssid_buf;
  is_secure = false; // ECN not available from AT+CWJAP?
  return ReturnCode::AnsweredRequest;
}

/**
 * @brief   Obtain the IP address of a station connected to the ESP32 SoftAP.
 * @details Uses @c AT+CWLIF which lists one entry per connected client.
 *          Only the first entry is returned.  Run the ESP32 in SoftAP mode
 *          (@ref setWiFiMode with @ref ESP::ApMode::SoftAP or
 *          @ref ESP::ApMode::StationSoftAP) before calling this method.
 *
 * @param[out] ip  Populated with the client IP string (e.g. "192.168.4.2") on success.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : @p ip is valid.
 *   - @ref ReturnCode::ErrorInvalidState : No clients connected or parse failed.
 *   - @ref ReturnCode::ErrorTimeout      : No response within the default timeout.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::getIpAddress(String& ip) noexcept
{
  m_tx_string << "AT+CWLIF";
  const ReturnCode rc = execCommand("+CWLIF:");
  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }

  // Response: +CWLIF:<ip>,<mac>
  char ip_buf[16] = { };
  if (m_rx_string.scanf("+CWLIF:%15[^,]", ip_buf) < 1)
  {
    return ReturnCode::ErrorInvalidState;
  }

  ip.clear();
  ip << ip_buf;
  return ReturnCode::AnsweredRequest;
}

/**
 * @brief   Start a Wi-Fi AP scan (AT+CWLAP).
 * @details Sends @c AT+CWLAP and returns immediately.  Each @c +CWLAP:(...)
 *          result line arrives asynchronously as a URC and is forwarded to
 *          @ref onListAp.  Register an @ref EventCallback with
 *          @ref Event::ListAp to receive individual AP entries.
 *
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Command transmitted successfully.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 *
 * @note    Results may arrive for several seconds.  The scan finishes
 *          when no further ListAp events occur.
 */
ReturnCode EspAt::listAp(Span<EspAt::ApInfo>ap_array, uint8_t& count) noexcept

{
  m_tx_string << "AT+CWLAP";

  auto rc = execCommandMulti("+CWLAP:", ESP::kListApTimeout);

  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }

  StringList ap_list(m_temp_buffer);
  ap_list.append(m_rx_string);
  ap_list.split('\n'); // Split the multi-line response into individual entries

  count = 0;
  int index = 0;
  //const char* result = m_rx_string.data();

  for ( auto &ap_info : ap_array )
  {
    if (ap_list.count() == 0)
    {
      break; // No more entries to process
    }

    int encryption = 0;
    int rssi = 0;

    if (sscanf(ap_list[index], "+CWLAP:(%d,\"%31[^\"]\",%d,", &encryption,
        ap_info.ssid, &rssi) == 3)
    {
      ap_info.encryption = static_cast<ApEncryption>(encryption);
      // Convert RSSI from dBm to a percentage (0-100) for easier interpretation
      ap_info.rssi = rssi <= -100 ? 0 : (rssi >= -50 ? 100 : 2 * (rssi + 100));
      ++count;
    }

    index++;
  }

  return rc;
}

/**
 * @brief   Read the ESP32 internal temperature sensor (AT+SYSTEMP?).
 * @details Avoids @c scanf @c %f (disabled in newlib-nano) by locating the
 *          colon delimiter and parsing with @c strtof via @ref String::toFloat.
 *
 * @param[out] temperature  Chip temperature in degrees Celsius on success.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : @p temperature is valid.
 *   - @ref ReturnCode::ErrorInvalidState : Response received but could not be parsed.
 *   - @ref ReturnCode::ErrorTimeout      : No response within the default timeout.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::getTemperature(float& temperature) noexcept
{
  m_tx_string << "AT+SYSTEMP?";
  const ReturnCode rc = execCommand("+SYSTEMP:");
  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }

  const std::size_t colon = m_rx_string.find(':');
  if (colon == String::npos)
  {
    return ReturnCode::ErrorInvalidState;
  }

  char temp_buf[16] = { };
  String num(temp_buf);
  num.append(m_rx_string.data() + colon + 1U);

  if (!num.toFloat(temperature))
  {
    return ReturnCode::ErrorInvalidState;
  }

  return ReturnCode::AnsweredRequest;
}

// ============================================================================
// TCP / IP commands
// ============================================================================

/**
 * @brief   Configure SNTP time zone and server (AT+CIPSNTPCFG).
 * @details Sends @c AT+CIPSNTPCFG=1,<tz>,"<server>" to enable SNTP with a
 *          single server.  The @ref ESP::TimeZone value encodes the UTC offset
 *          as an integer (e.g. @c 8 for UTC+08:00, @c -5 for UTC-05:00,
 *          @c 530 for UTC+05:30).
 *
 * @param[in] tz           UTC offset (see @ref ESP::TimeZone).
 * @param[in] sntp_server  NTP server hostname (e.g. "pool.ntp.org").
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : SNTP configured; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : No response within the default timeout.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 *
 * @note    The @ref Event::Time event is fired asynchronously (as a URC) once
 *          the module has synchronised its clock with the configured server.
 */
ReturnCode EspAt::setTimeZoneAndSNTP(
  ESP::TimeZone tz, String& sntp_server) noexcept
{
  m_tx_string.printf("AT+CIPSNTPCFG=1,%d,\"%s\"", static_cast<int>(tz),
      sntp_server.data());
  return execCommand(kExpectedOk);
}

/**
 * @brief   Ping a remote host (AT+PING).
 * @details Sends @c AT+PING="<host>" and waits for either @c +PING:<ms>
 *          (success) or @c +PING:TIMEOUT (failure).
 *
 * @param[in] host        Remote hostname or IP address string.
 * @param[in] timeout_ms  Maximum time to wait for a ping reply, in milliseconds.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Reply received; "+PING:<ms>" in response.
 *   - @ref ReturnCode::ErrorInvalidState : Firmware replied "+PING:TIMEOUT" or "ERROR".
 *   - @ref ReturnCode::ErrorTimeout      : No response within @p timeout_ms.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::ping(String& host, uint32_t timeout_ms) noexcept
{
  m_tx_string.printf("AT+PING=\"%s\"", host.data());
  const ReturnCode rc = execCommand("+PING:", timeout_ms);
  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }

  // Distinguish success (+PING:<ms>) from failure (+PING:TIMEOUT).
  if (m_rx_string.contains("TIMEOUT"))
  {
    return ReturnCode::ErrorInvalidState;
  }

  return ReturnCode::AnsweredRequest;
}

// ============================================================================
// MQTT commands
// ============================================================================

/**
 * @brief   Set MQTT scheme and client ID via AT+MQTTUSERCFG.
 * @details Builds @c AT+MQTTUSERCFG=0,<scheme>,"<client_id>","","",0,0,"".
 *          Username and password are left empty; use @ref setMqttUsername and
 *          @ref setMqttPassword if they are required.
 *
 * @param[in] scheme     Connection scheme:
 *                        0 = MQTT over TCP,
 *                        1 = MQTT over TLS (no certificate verification),
 *                        2 = MQTT over TLS (server cert verification),
 *                        3 = MQTT over TLS (client & server cert),
 *                        4 = MQTT over WebSocket,
 *                        5 = MQTT over WebSocket with TLS.
 * @param[in] client_id  Null-terminated MQTT client identifier (max 64 chars).
 *                        For longer IDs use @ref setMqttClientId.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Configuration accepted; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : No response within the default timeout.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::setMqttUserConfig(int scheme, const char* client_id) noexcept
{
  m_tx_string.printf("AT+MQTTUSERCFG=0,%d,\"%s\",\"\",\"\",0,0,\"\"", scheme,
      client_id);
  return execCommand(kExpectedOk);
}

/**
 * @brief   Set a long MQTT client ID (AT+MQTTLONGCLIENTID — two-phase protocol).
 * @details Phase 1: sends @c AT+MQTTLONGCLIENTID=0,<len> and waits for the
 *          @c > prompt.  Phase 2: transmits the client ID bytes raw and waits
 *          for "OK".  Use this when the client ID exceeds 64 characters.
 *
 * @param[in] client_id  String whose content and size define the client ID.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Client ID accepted; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : Timeout waiting for ">" prompt or "OK".
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::setMqttClientId(String& client_id) noexcept
{
  m_tx_string.printf("AT+MQTTLONGCLIENTID=0,%u",
      static_cast<unsigned>(client_id.size()));
  ReturnCode rc = execCommand(">", kDefaultTimeoutMs);
  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }
  return execLongDataCommand(client_id.data(), client_id.size());
}

/**
 * @brief   Set a long MQTT username (AT+MQTTLONGUSERNAME — two-phase protocol).
 * @details Phase 1: sends @c AT+MQTTLONGUSERNAME=0,<len> and waits for @c >.
 *          Phase 2: transmits the username bytes raw and waits for "OK".
 *
 * @param[in] username  String whose content and size define the username.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Username accepted; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : Timeout waiting for ">" prompt or "OK".
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::setMqttUsername(String& username) noexcept
{
  m_tx_string.printf("AT+MQTTLONGUSERNAME=0,%u",
      static_cast<unsigned>(username.size()));
  ReturnCode rc = execCommand(">", kDefaultTimeoutMs);
  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }
  return execLongDataCommand(username.data(), username.size());
}

/**
 * @brief   Set a long MQTT password (AT+MQTTLONGPASSWORD — two-phase protocol).
 * @details Phase 1: sends @c AT+MQTTLONGPASSWORD=0,<len> and waits for @c >.
 *          Phase 2: transmits the password bytes raw and waits for "OK".
 *
 * @param[in] password  String whose content and size define the password.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Password accepted; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : Timeout waiting for ">" prompt or "OK".
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::setMqttPassword(String& password) noexcept
{
  m_tx_string.printf("AT+MQTTLONGPASSWORD=0,%u",
      static_cast<unsigned>(password.size()));
  ReturnCode rc = execCommand(">", kDefaultTimeoutMs);
  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }
  return execLongDataCommand(password.data(), password.size());
}

/**
 * @brief   Connect to an MQTT broker (AT+MQTTCONN).
 * @details Sends @c AT+MQTTCONN=0,"<endpoint>",<port>,0 with auto-reconnect
 *          disabled.  Waits up to 10 s for "OK".  The @c +MQTTCONNECTED:0
 *          URC arrives asynchronously and is handled by @ref onMqttConn.
 *
 * @param[in] endpoint  Broker hostname or IP address string.
 * @param[in] port      Broker TCP port (e.g. 1883 for plain MQTT, 8883 for TLS).
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Connection initiated; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : No "OK" within 10 s.
 *   - @ref ReturnCode::ErrorGeneral      : Firmware returned "ERROR".
 */
ReturnCode EspAt::mqttConnect(String& endpoint, uint16_t port) noexcept
{
  m_tx_string.printf("AT+MQTTCONN=0,\"%s\",%u,0", endpoint.data(),
      static_cast<unsigned>(port));
  return execCommandMulti(kExpectedOk, 10000U);
}

/**
 * @brief   Publish an MQTT message as a quoted string (AT+MQTTPUB).
 * @details Sends @c AT+MQTTPUB=0,"<topic>","<msg>",0,0 with QoS 0 and
 *          retain=0.  The message must not contain double-quote (@c ")
 *          characters or embedded null bytes.
 *
 * @param[in] topic  MQTT topic string.
 * @param[in] msg    Message payload (null-terminated, no double-quotes).
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Message published; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : No response within the default timeout.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 *
 * @warning @p msg must not contain @c " characters; use @ref mqttPublishRaw
 *          for arbitrary binary or JSON payloads.
 */
ReturnCode EspAt::mqttPublish(String& topic, String& msg) noexcept
{
  m_tx_string.printf("AT+MQTTPUB=0,\"%s\",\"%s\",0,0", topic.data(),
      msg.data());
  return execCommand(kExpectedOk, 5000U);
}

/**
 * @brief   Publish a raw binary MQTT payload (AT+MQTTPUBRAW — two-phase protocol).
 * @details Phase 1: sends @c AT+MQTTPUBRAW=0,"<topic>",<len>,0,0 and waits
 *          for the @c > prompt.  Phase 2: transmits @p len bytes from @p msg
 *          and waits for "OK".  Supports arbitrary binary payloads including
 *          embedded null bytes as long as the underlying UART write is
 *          length-aware.
 *
 * @param[in] topic  MQTT topic string.
 * @param[in] msg    Pointer to the payload buffer.
 * @param[in] len    Number of bytes to publish.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Payload published; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : Timeout waiting for ">" prompt or "OK".
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::mqttPublishRaw(
  String& topic, uint8_t* msg, size_t len) noexcept
{
  m_tx_string.printf("AT+MQTTPUBRAW=0,\"%s\",%u,0,0", topic.data(),
      static_cast<unsigned>(len));
  ReturnCode rc = execCommand(">", kDefaultTimeoutMs);
  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }
  return execLongDataCommand(reinterpret_cast<const char*>(msg), len, 5000U);
}

/**
 * @brief   Subscribe to an MQTT topic (AT+MQTTSUB) with QoS 0.
 * @details Sends @c AT+MQTTSUB=0,"<topic>",0.  Inbound messages on this
 *          topic will arrive as @c +MQTTSUBRECV URCs handled by
 *          @ref onMqttRecvEvent.
 *
 * @param[in] topic  MQTT topic filter string (may include wildcards @c + and @c #).
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Subscribed; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : No response within the default timeout.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::mqttSubscribe(String& topic) noexcept
{
  m_tx_string.printf("AT+MQTTSUB=0,\"%s\",0", topic.data());
  return execCommand(kExpectedOk);
}

/**
 * @brief   Unsubscribe from an MQTT topic (AT+MQTTUNSUB).
 * @details Sends @c AT+MQTTUNSUB=0,"<topic>".
 *
 * @param[in] topic  MQTT topic filter previously passed to @ref mqttSubscribe.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Unsubscribed; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : No response within the default timeout.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::mqttUnsubscribe(String& topic) noexcept
{
  m_tx_string.printf("AT+MQTTUNSUB=0,\"%s\"", topic.data());
  return execCommand(kExpectedOk);
}

/**
 * @brief   Close the MQTT connection and release firmware resources (AT+MQTTCLEAN).
 * @details Sends @c AT+MQTTCLEAN=0.  After this call a new connection can be
 *          established by calling @ref setMqttUserConfig and @ref mqttConnect.
 *
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Connection closed; "OK" received.
 *   - @ref ReturnCode::ErrorTimeout      : No response within the default timeout.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer fault or UART write error.
 */
ReturnCode EspAt::mqttCLose() noexcept
{
  m_tx_string << "AT+MQTTCLEAN=0";
  return execCommand(kExpectedOk);
}

// ============================================================================
// Private — hardware control
// ============================================================================

/**
 * @brief   Assert the reset pin, clear all state flags and re-arm DMA.
 * @details Pulls @ref m_resetPin low for 100 ms then high.  The module takes
 *          a short time to boot; the caller should wait for the "ready" URC
 *          via @ref m_ready before sending any AT commands.
 *
 * @note    Must be called from task context.
 */
void EspAt::reset() noexcept
{
  m_ready = false;
  m_initialized = false;
  m_wifi_connected = false;
  m_mqtt_connected = false;

  (void) Uart::readDMA(m_rx_string, UartMode::ToIdle);

  m_resetPin.write(false);
  hel::sleep(100);
  m_resetPin.write(true);
  hel::sleep(100);
}

// ============================================================================
// Private — URC dispatch
// ============================================================================

/**
 * @brief   Dispatch the oldest pending URC to its registered handler.
 * @details
 *   - Drains one entry from @ref m_urc_buffer per call.
 *   - Wraps the raw char array in a @ref String view, trims whitespace, then
 *     dispatches to the matching @c onXxx handler by keyword search.
 *   - Unrecognised URCs are silently discarded.
 *   - Must be called regularly from task context (e.g. the main run loop).
 *
 * @warning Must not be called from ISR context.
 */
void EspAt::processURC() noexcept
{
  if (m_urc_buffer.receive(m_temp_buffer, 10) != ReturnCode::AnsweredRequest)
  {
    return;
  }

  String urc_string(m_temp_buffer);
  urc_string.sync();

  if (urc_string.empty())
  {
    return;
  }

  if (urc_string.contains("ready"))
  {
    onReady(urc_string);
  }
  else if (urc_string.contains("WIFI CONNECTED")
      || urc_string.contains("WIFI GOT IP"))
  {
    onConnected(urc_string);
  }
  else if (urc_string.contains("TIME_UPDATED"))
  {
    onTimeUpdated(urc_string);
  }
  else if (urc_string.contains("MQTTCONNECTED")
      || urc_string.contains("MQTTDISCONNECTED"))
  {
    onMqttConn(urc_string);
  }
  else if (urc_string.contains("MQTTSUBRECV"))
  {
    onMqttRecvEvent(urc_string);
  }
  else if (urc_string.contains("CWLAP"))
  {
    onListAp(urc_string);
  }
}

// ============================================================================
// Private — UART ISR callback
// ============================================================================

/**
 * @brief   UART event handler; called from ISR context by the UART driver.
 * @details On @ref UartEvent::RxComplete:
 *            1. Syncs the @ref m_rx_string size from the DMA counter.
 *            2. Ignores bare @c "\r\n" frames (echo artefacts / keep-alives).
 *            3. If @ref waitForResponse is pending, signals it by clearing
 *               @ref m_waiting_for_response so the task loop can evaluate
 *               @ref m_rx_string directly.
 *            4. Otherwise copies the frame into @ref m_urc_buffer for
 *               deferred processing by @ref processURC.
 *            5. Re-arms DMA-to-idle for the next frame.
 *          All other events re-arm DMA without further action.
 *
 * @param[in] event  UART event type.
 * @param[in] count  Bytes transferred (unused; length derived from sync).
 *
 * @warning Called from ISR context — must not block or call any RTOS API
 *          that is not ISR-safe.
 */
void EspAt::handleEvent(UartEvent event, uint16_t count) noexcept
{
  (void) count;

  if (event != UartEvent::RxComplete)
  {
    (void) Uart::readDMA(m_rx_string, UartMode::ToIdle);
    return;
  }

  m_rx_string.sync();

  const bool is_bare_crlf = (m_rx_string.size() == 2U)
      && (m_rx_string.data()[0] == '\r') && (m_rx_string.data()[1] == '\n');

  if (!is_bare_crlf)
  {
    if (m_waiting_for_response)
    {
      // Unblock waitForResponse — it reads m_rx_string directly.
      m_waiting_for_response = false;
    }
    else
    {
      (void) m_urc_buffer.sendToBackFromIsr(m_rx_buffer);
    }
  }

  // Re-arm DMA for the next frame.
  (void) Uart::readDMA(m_rx_string, UartMode::ToIdle);
}

// ============================================================================
// Private — command execution helpers
// ============================================================================

/**
 * @brief   Append "\r\n" to @ref m_tx_string, transmit and wait for one frame.
 * @details Calls @ref waitForResponse internally.  Returns
 *          @ref ReturnCode::ErrorInvalidState if the first received frame does
 *          not contain @p expected_response.  Use @ref execCommandMulti when
 *          the firmware sends intermediate frames before the final response.
 *
 * @param[in] expected_response  Sub-string to look for in the reply.
 * @param[in] timeout_ms         Maximum wait time in milliseconds.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Response contained @p expected_response.
 *   - @ref ReturnCode::ErrorInvalidState : Frame received but did not match.
 *   - @ref ReturnCode::ErrorTimeout      : No response within @p timeout_ms.
 *   - @ref ReturnCode::ErrorGeneral      : @ref m_tx_string was empty or full,
 *                                          or the UART write failed.
 */
ReturnCode EspAt::execCommand(
  const char* expected_response, uint32_t timeout_ms) noexcept
{
  if (m_tx_string.empty() || m_tx_string.remaining() < 2U)
  {
    return ReturnCode::ErrorGeneral;
  }

  m_tx_string << "\r\n";

  m_rx_string.clear();
  const ReturnCode rc = Uart::write(m_tx_string, kDefaultTimeoutMs);
  m_tx_string.clear();

  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }

  if (expected_response == nullptr)
  {
    return ReturnCode::AnsweredRequest;
  }

  return waitForResponse(expected_response, timeout_ms);
}

/**
 * @brief   Like @ref execCommand but tolerates intermediate frames.
 * @details Appends "\r\n", sends, then calls @ref waitForResponseMulti which
 *          re-arms the wait loop whenever a non-matching, non-error frame
 *          arrives.  Suitable for commands such as @c AT+CWJAP that produce
 *          "WIFI CONNECTED" before the final "OK".
 *
 * @param[in] expected_response  Sub-string to look for in any received frame.
 * @param[in] timeout_ms         Total maximum wait time across all frames.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : A frame containing @p expected_response received.
 *   - @ref ReturnCode::ErrorGeneral      : Firmware replied with "ERROR", or TX fault.
 *   - @ref ReturnCode::ErrorTimeout      : Timeout elapsed before the expected frame arrived.
 */
ReturnCode EspAt::execCommandMulti(
  const char* expected_response, uint32_t timeout_ms) noexcept
{
  if (m_tx_string.empty() || m_tx_string.remaining() < 2U)
  {
    return ReturnCode::ErrorGeneral;
  }

  m_tx_string << "\r\n";

  m_rx_string.clear();
  const ReturnCode rc = Uart::write(m_tx_string, kDefaultTimeoutMs);
  m_tx_string.clear();

  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }

  return waitForResponseMulti(expected_response, timeout_ms);
}

/**
 * @brief   Send raw bytes via @ref m_tx_string and wait for "OK".
 * @details This is phase 2 of the AT LONG-data protocol.  The caller must have
 *          already sent the header command (e.g. @c AT+MQTTLONGCLIENTID=0,<n>)
 *          and received the @c > prompt before invoking this method.
 *          The data is loaded into @ref m_tx_string and transmitted WITHOUT
 *          appending @c "\r\n".
 *
 * @param[in] data        Pointer to the byte array to transmit.
 * @param[in] len         Number of bytes to send.
 * @param[in] timeout_ms  Maximum wait time for the "OK" acknowledgement.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : "OK" received after data transmission.
 *   - @ref ReturnCode::ErrorTimeout      : No "OK" within @p timeout_ms.
 *   - @ref ReturnCode::ErrorGeneral      : UART write error.
 */
ReturnCode EspAt::execLongDataCommand(
  const char* data, std::size_t len, uint32_t timeout_ms) noexcept
{
  m_tx_string.clear();
  m_tx_string.append(data, len);

  m_rx_string.clear();
  const ReturnCode rc = Uart::write(m_tx_string, kDefaultTimeoutMs);
  m_tx_string.clear();

  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }

  return waitForResponse(kExpectedOk, timeout_ms);
}

/**
 * @brief   Busy-wait until one frame containing @p expected_response arrives.
 * @details Sets @ref m_waiting_for_response; the ISR clears it when any
 *          non-CRLF frame is received.  The task then evaluates
 *          @ref m_rx_string.
 *
 * @param[in] expected_response  Sub-string to locate in @ref m_rx_string.
 * @param[in] timeout_ms         Maximum wait time in milliseconds.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : @ref m_rx_string contained @p expected_response.
 *   - @ref ReturnCode::ErrorInvalidState : Frame received but did not match.
 *   - @ref ReturnCode::ErrorTimeout      : Timeout elapsed before any frame arrived.
 *
 * @note    Busy-waits the CPU.  In a fully RTOS-based design this loop should
 *          pend on a semaphore instead.
 */
ReturnCode EspAt::waitForResponse(
  const char* expected_response, uint32_t timeout_ms) noexcept
{
  m_cmd_timeout.start(timeout_ms);
  m_waiting_for_response = true;

  do
  {
    if (!m_waiting_for_response)
    {
      return
          m_rx_string.contains(expected_response) ?
              ReturnCode::AnsweredRequest : ReturnCode::ErrorInvalidState;
    }
    sleep(10);
  }
  while (!m_cmd_timeout.expired());

  m_waiting_for_response = false;
  return ReturnCode::ErrorTimeout;
}

/**
 * @brief   Busy-wait across multiple frames until @p expected_response is found.
 * @details Each time a non-matching frame arrives, the wait is re-armed so
 *          the next frame can be evaluated.  The loop terminates on:
 *          - A frame containing @p expected_response → @ref ReturnCode::AnsweredRequest.
 *          - A frame containing "ERROR" → @ref ReturnCode::ErrorGeneral.
 *          - Overall timeout → @ref ReturnCode::ErrorTimeout.
 *
 * @param[in] expected_response  Sub-string to locate in any received frame.
 * @param[in] timeout_ms         Total wall-clock limit across all frames.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest : Matching frame received.
 *   - @ref ReturnCode::ErrorGeneral   : "ERROR" frame received.
 *   - @ref ReturnCode::ErrorTimeout   : Deadline elapsed.
 */
ReturnCode EspAt::waitForResponseMulti(
  const char* expected_response, uint32_t timeout_ms) noexcept
{
  m_cmd_timeout.start(timeout_ms);

  while (!m_cmd_timeout.expired())
  {
    m_waiting_for_response = true;

    // Spin until a frame arrives or the overall deadline expires.
    while (m_waiting_for_response && !m_cmd_timeout.expired())
    {
    }

    if (!m_waiting_for_response) // A frame arrived.
    {
      if (m_rx_string.contains(expected_response))
      {
        return ReturnCode::AnsweredRequest;
      }
      if (m_rx_string.contains("ERROR"))
      {
        return ReturnCode::ErrorGeneral;
      }
      // Intermediate frame — loop and wait for the next one.
    }
  }

  m_waiting_for_response = false;
  return ReturnCode::ErrorTimeout;
}

// ============================================================================
// Private — configuration helpers
// ============================================================================

/**
 * @brief   Enable or disable the command echo (ATE1 / ATE0).
 *
 * @param[in] enable  @c true to echo commands; @c false to suppress echo.
 * @return  ReturnCode — same values as @ref execCommand.
 */
ReturnCode EspAt::setEcho(bool enable) noexcept
{
  m_tx_string << "ATE" << (enable ? "1" : "0");
  return execCommand(kExpectedOk);
}

// ============================================================================
// Private — URC event handlers
// ============================================================================

/**
 * @brief   Handle the "ready" URC — fired after each ESP32 reset.
 * @details Sets @ref m_ready to allow @ref stateInit to proceed with
 *          post-reset configuration.
 *
 * @param[in] msg  URC string; must contain "ready".
 */
void EspAt::onReady(String& msg) noexcept
{
  if (msg.contains("ready"))
  {
    m_ready = true;
  }
}

/**
 * @brief   Handle Wi-Fi link state change URCs.
 * @details Sets @ref m_wifi_connected on "WIFI CONNECTED" and clears it on
 *          "WIFI DISCONNECT" (which covers both "WIFI DISCONNECTED" and the
 *          abbreviated variant).
 *
 * @param[in] msg  URC string containing "WIFI CONNECTED" or "WIFI DISCONNECT".
 */
void EspAt::onConnected(String& msg) noexcept
{
  if(msg.contains("WIFI CONNECTED") || msg.contains("WIFI CONNECT"))
  {
    m_event_callback(Event::WiFiConnection, msg); // Notify application of Wi-Fi connection.
    m_wifi_connected = true;
  }
  else if (msg.contains("WIFI DISCONNECT"))
  {
    m_wifi_connected = false;
  }
}

/**
 * @brief   Handle "WIFI GOT IP" and "TIME_UPDATED" URCs.
 * @details "WIFI GOT IP" confirms that the DHCP lease has been obtained —
 *          the station is fully online.  "TIME_UPDATED" indicates that the
 *          SNTP synchronisation has succeeded.
 *
 * @param[in] msg  URC string containing "WIFI GOT IP" or "TIME_UPDATED".
 */
void EspAt::onTimeUpdated(String& msg) noexcept
{
  if (msg.contains("WIFI GOT IP"))
  {
    m_wifi_connected = true;
  }
  // "TIME_UPDATED": time is now valid; Calendar integration can be added here.
}

/**
 * @brief   Handle MQTT broker connection state URCs.
 * @details Sets @ref m_mqtt_connected on "+MQTTCONNECTED:0" and clears it
 *          on "+MQTTDISCONNECTED:0".
 *
 * @param[in] msg  URC string containing "MQTTCONNECTED" or "MQTTDISCONNECTED".
 */
void EspAt::onMqttConn(String& msg) noexcept
{
  m_mqtt_connected = msg.contains("MQTTCONNECTED");
}

/**
 * @brief   Handle inbound MQTT message URCs (+MQTTSUBRECV).
 * @details The URC format is:
 *          @code
 *          +MQTTSUBRECV:0,"<topic>",<len>,<data>
 *          @endcode
 *          Parsing and forwarding to the application layer can be implemented
 *          here by registering an @ref EventCallback.
 *
 * @param[in] msg  URC string starting with "+MQTTSUBRECV".
 */
void EspAt::onMqttRecvEvent(String& msg) noexcept
{
  (void) msg;
  // TODO: parse "+MQTTSUBRECV:0,"<topic>",<len>,<payload>" and forward
  //       to application via an EventCallback (Event::MqttRecv).
}

/**
 * @brief   Handle AP scan result URCs (+CWLAP).
 * @details Each URC contains one AP entry in the format:
 *          @code
 *          +CWLAP:(<ecn>,"<ssid>",<rssi>,"<mac>",<ch>,...)
 *          @endcode
 *          Accumulate entries or forward them to the application via
 *          an @ref EventCallback (Event::ListAp).
 *
 * @param[in] msg  URC string starting with "+CWLAP".
 */
void EspAt::onListAp(String& msg) noexcept
{
  (void) msg;
  // TODO: parse "+CWLAP:(<ecn>,"ssid",rssi,...)" and fire Event::ListAp
  //       via an EventCallback registered by the application.
}

// ============================================================================
// FSM state handlers
// ============================================================================

/**
 * @brief   Initial FSM state — reset the module and perform first-run config.
 * @details On the first entry: asserts reset via @ref reset.  Waits for
 *          @ref m_ready.  Once ready: disables echo and reads the chip
 *          temperature as a connectivity sanity check.  On success sets
 *          @ref m_initialized so the FSM transitions to @ref stateIdle.
 *          On timeout: re-asserts reset and retries.
 */
void EspAt::stateInit() noexcept
{
  static bool first_entry = true;

  if (first_entry)
  {
    reset();
    m_cmd_timeout.start(kinitTimeoutMs);
    first_entry = false;
  }

  if (m_ready)
  {
    hel::sleep(kSmallDelayMs);

    ReturnCode rc = setEcho(false);
    (void) rc;

    hel::sleep(kSmallDelayMs);

    float temperature = 0.0F;
    rc = getTemperature(temperature);

    if (rc != ReturnCode::AnsweredRequest)
    {
      return;
    }

    hel::sleep(kSmallDelayMs);

    rc = setWiFiMode(ESP::ApMode::Station);
    if (rc != ReturnCode::AnsweredRequest)
    {
      return;
    }

    String str(m_temp_buffer);
    m_event_callback(Event::Ready, str); // Notify application that we're ready to accept commands.

    m_initialized = true;
    first_entry = true; // Reset flag for next stateInit entry (e.g. after reboot).
  }
  else if (m_cmd_timeout.expired())
  {
    first_entry = true; // Allow reset on next entry.
    reset();
    m_cmd_timeout.start(kinitTimeoutMs);
  }
}

/**
 * @brief   Idle FSM state — normal operating state after successful init.
 * @details Command methods (@ref connectWiFi, @ref mqttConnect, etc.) are
 *          expected to be called directly by the application from a separate
 *          task or via callbacks.  This handler is reserved for any periodic
 *          housekeeping (keepalive pings, watchdog resets, etc.).
 */
void EspAt::stateIdle() noexcept
{
  // Reserved for periodic housekeeping (e.g. keepalive, watchdog, etc.).
}

} // namespace hel
