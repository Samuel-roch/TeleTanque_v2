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
#include "esp_at.hpp"

namespace hel
{

Esp32BasicCmds::Esp32BasicCmds(EspAt& m_at_client)
    : m_at_client(m_at_client)
{
  // TODO Auto-generated constructor stub

}


ReturnCode Esp32BasicCmds::at()
{
  // AT: Test AT Startup
  m_at_client.txBuffer() << "AT\r\n";

  auto rc = m_at_client.execCommand(kExpectedOk);

  return rc;
}

} /* namespace drv */
