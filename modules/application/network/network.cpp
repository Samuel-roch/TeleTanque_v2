/**
 ******************************************************************************
 * @file    network.cpp
 * @author  SENAI CIMATEC (AEE Selene Team)
 * @version 0.0.1
 * @date    Apr 19, 2026
 * @brief   
 *
 ******************************************************************************
 */
#include "network.hpp"

using namespace hel;

Network::Network() noexcept
    :
      StaticTask("Network", TaskPriority::Normal),
      m_esp_at(BSP::esp32_at, BSP::esp32_reset)
{

  EspAt::EventCallback callback(this, &Network::onEspAtEvent);
  m_esp_at.setEventCallback(callback);

  m_esp_ready = false;
  m_esp_wifi_connected = false;

}

void Network::run() noexcept
{
  (void) TaskBase::notifyTake();

  m_esp_at.notify();


  EspAt::ApInfo default_ap_inf[10];
  Span<EspAt::ApInfo> ap_info_span(default_ap_inf);


  uint8_t ap_count = 0;
  while (true)
  {
    // true if notified, run operations immediately;
    // false if timeout, run periodic operations.
    if(notifyTake(kRunIntervalMs) == true)
    {
      // Handle notifications if needed
    }
    else // Verify Wi-Fi connection periodically
    {
      if (m_esp_ready && !m_esp_wifi_connected)
      {
         if(m_esp_at.listAp(ap_info_span, ap_count) == ReturnCode::AnsweredRequest)
         {
           for (uint8_t i = 0; i < ap_count; ++i)
           {
             const auto& ap = ap_info_span[i];

             String ssid_str(ap.ssid);
             ssid_str.sync();

             if(ap.ssid == kWifiSsid)
             {
               (void)m_esp_at.connectWiFi(String(kWifiSsid), String(kWifiPassword));
               break;
             }
           }
         }
      }
    }
  }
}

void Network::onEspAtEvent(EspAt::Event event, String& data) noexcept
{
  switch (event)
  {
    case EspAt::Event::Ready :
      m_esp_ready = true;
      break;

    case EspAt::Event::WiFiConnection :
      m_esp_wifi_connected = data.contains("CONNECTED");
      break;

    case EspAt::Event::TimeUpdated :

      break;

      default :
        break;
  }
}
