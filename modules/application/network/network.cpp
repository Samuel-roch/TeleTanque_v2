/**
 ******************************************************************************
 * @file    network.cpp
 * @author  SENAI CIMATEC (AEE Selene Team)
 * @version 0.0.1
 * @date    Apr 19, 2026
 * @brief   
 *
 ******************************************************************************
 *
 * @attention
 *
 * Copyright (c) 2026 SENAI CIMATEC (AEE team).
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
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
