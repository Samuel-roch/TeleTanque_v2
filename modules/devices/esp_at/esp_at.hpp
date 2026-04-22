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

#include <hel_at_client>
#include "setup.hpp"

#include "esp_basic_cmds.hpp"
#include "esp_wifi_cmds.hpp"

namespace hel
{

class EspAt :
  public AtClient<kEspAtBuffSize, kEspAtURCBufSize, kEspAtUrcListSize>
{
public:
  EspAt(UART_handle& handle)
      :
      AtClient(handle)
  {

  }

private:
  Esp32BasicCmds m_basic_cmds { *this };
};

} /* namespace dev */

#endif /* DEVICES_ESP_AT_ESP_AT_HPP_ */
