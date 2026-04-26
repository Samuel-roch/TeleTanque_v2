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

Network::Network() noexcept :
    StaticTask("Network", TaskPriority::Normal)
    , m_at_client(BSP::esp32_at, BSP::esp32_reset)
{

}

void Network::run() noexcept
{
  (void)TaskBase::notifyTake();

  m_at_client.notify();


  while (true)
  {
    sleep(100);
  }
}
