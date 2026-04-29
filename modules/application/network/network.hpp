/**
 ******************************************************************************
 * @file    network.hpp
 * @author  Samuel Almeida
 * @version 0.0.1
 * @date    Apr 19, 2026
 * @brief   
 *
 ******************************************************************************
 *
 ******************************************************************************
 */
#ifndef APPLICATION_NETWORK_NETWORK_HPP_
#define APPLICATION_NETWORK_NETWORK_HPP_

#include <hel_target>
#include <hel_string>

#include "esp_at/esp_at.hpp"
#include "setup.hpp"

class Network :
  public hel::StaticTask<kNetworkStackSize>
{
public:

  static Network& instance() noexcept
  {
    static Network instance;
    return instance;
  }



private: //  Constants
  static constexpr uint32_t kRunIntervalMs = 10000U; // Main loop interval 10s.
  static inline const char * kWifiSsid = "S25";
  static inline const char * kWifiPassword = "12345678";

private: // Members
  hel::EspAt m_esp_at;

  bool m_esp_wifi_connected;
  bool m_esp_ready;


private:  // Methods
  explicit Network() noexcept;
  void run() noexcept override;

  void onEspAtEvent(hel::EspAt::Event event, hel::String& data) noexcept;



};

#endif /* APPLICATION_NETWORK_NETWORK_HPP_ */
