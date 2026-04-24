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
#include <network/network.hpp>

using namespace hel;

Network& s_network = Network::instance();

Network::Network() noexcept :
    StaticTask("Network", TaskPriority::Normal)
{

}

void Network::run() noexcept
{
  while (true)
  {
    delay(1000);
  }
}
