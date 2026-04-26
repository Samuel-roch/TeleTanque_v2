/**
 ******************************************************************************
 * @file    setup.hpp
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
#ifndef APPLICATION_SETUP_HPP_
#define APPLICATION_SETUP_HPP_

// Library FreeRTOS
#include "freertos/task.hpp"

// Helios Drivers
#include "uart/uart.hpp"
#include "gpio/gpio.hpp"

// HAL Drivers
#include "usart.h"
#include "gpio.h"

constexpr hel::UBaseType kNetworkStackSize = 512U;
constexpr hel::UBaseType kTeletanqueStackSize = 512U;

class BSP final
{
public:

  static inline hel::Gpio enable_ldo  {IoPortPin::PF10};
  static inline hel::Gpio led {IoPortPin::PG3};

  static inline UART_handle& esp32_at {huart2};
  static inline hel::Gpio esp32_reset {IoPortPin::PD4};
};

#endif /* APPLICATION_SETUP_HPP_ */
