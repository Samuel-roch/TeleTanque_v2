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

#ifndef DEVICES_ESP_AT_ESP_WIFI_CMDS_HPP_
#define DEVICES_ESP_AT_ESP_WIFI_CMDS_HPP_


namespace hel
{

class EspAt;

class EspAtWifiCmds
{
public:
  EspAtWifiCmds(EspAt& m_at_client);


private:
  EspAt& m_at_client;
};

} /* namespace drv */

#endif /* DEVICES_ESP_AT_ESP_WIFI_CMDS_HPP_ */
