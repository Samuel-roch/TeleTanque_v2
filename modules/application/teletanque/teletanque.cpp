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
  while (true)
  {

    sleep(100);
  }
}
