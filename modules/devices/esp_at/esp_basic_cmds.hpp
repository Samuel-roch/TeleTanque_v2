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

#ifndef DEVICES_ESP_AT_ESP32BASICCMDS_HPP_
#define DEVICES_ESP_AT_ESP32BASICCMDS_HPP_

#include <hel_return_code>

namespace hel
{

class EspAt;

class Esp32BasicCmds
{
public:
  Esp32BasicCmds(EspAt& m_at_client);

  ReturnCode at();
  ReturnCode reset();
  ReturnCode setEcho(bool enable);
  ReturnCode getTemperature(float& temperature);

private:
  EspAt& m_at_client;

};

} /* namespace drv */

#endif /* DEVICES_ESP_AT_ESP32BASICCMDS_HPP_ */
