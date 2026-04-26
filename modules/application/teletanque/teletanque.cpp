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
#include "teletanque.hpp"

using namespace hel;

Teletanque& s_Teletanque = Teletanque::instance();

Teletanque::Teletanque() noexcept :
    StaticTask("Network", TaskPriority::Normal)
{

}

void Teletanque::run() noexcept
{
  BSP::enable_ldo.write(true);
  sleep(50); // Allow LDO to stabilize before accessing peripherals.

  m_network.notify();

  while (true)
  {
    BSP::led.toggle();
    sleep(500);
  }
}
