/**
 ******************************************************************************
 * @file
 * @author
 * @version
 * @date    Apr 21, 2026
 * @brief   
 *
 ******************************************************************************
 */
#include "esp_basic_cmds.hpp"
#include "esp_at_commons.hpp"

#include "esp_at.hpp"

namespace hel
{

using namespace ESP;

Esp32BasicCmds::Esp32BasicCmds(EspAt& m_at_client)
    :
    m_at_client(m_at_client)
{
  // TODO Auto-generated constructor stub

}

ReturnCode Esp32BasicCmds::at()
{
  // AT: Test AT Startup
  m_at_client.txBuffer() << "AT\r\n";

  // Expect "OK\r\n" in response to the AT command.
  auto rc = m_at_client.execCommand(kExpectedOk);
  return rc;
}

ReturnCode Esp32BasicCmds::reset()
{
  String &buffer = m_at_client.txBuffer();

  // AT+RST: Reset the ESP32
  buffer << "AT+RST\r\n";

  // Expect "ready\r\n" in response to the reset command.
  auto rc = m_at_client.execCommand("ready");
  return rc;
}
ReturnCode Esp32BasicCmds::setEcho(bool enable)
{
  String &buffer = m_at_client.txBuffer();

  // ATE1 od ATE0: Enable or disable command echo
  buffer << "ATE" << (enable ? "1" : "0");

  // Expect "ATE1" or "ATE0" in response to confirm the echo setting.
  auto rc = m_at_client.execCommand(kExpectedOk);
  return rc;
}

ReturnCode Esp32BasicCmds::getTemperature(float& temperature)
{
  String &buffer = m_at_client.txBuffer();

  // AT+SYSTEMP?: Get the current temperature reading from the ESP32
  buffer << "AT+SYSTEMP?";

  // Expect a response in the format "+SYSTEMP:<value>"
  auto rc = m_at_client.execCommand("+SYSTEMP:");
  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc; // Return error if the expected response was not received
  }

  // Parse the temperature value from the response.
  // Avoid scanf with %f — newlib-nano does not support float scanf by default.
  // Instead, locate the ':' and parse with strtof via String::toFloat.
  String &response = m_at_client.rxBuffer();

  const std::size_t colon = response.find(':');
  if (colon == String::npos)
  {
    return ReturnCode::ErrorInvalidState;
  }

  StringData<16> num;
  num.append(response.data() + colon + 1U);

  if (!num.toFloat(temperature))
  {
    return ReturnCode::ErrorInvalidState;
  }

  return ReturnCode::AnsweredRequest;
}

} /* namespace drv */
