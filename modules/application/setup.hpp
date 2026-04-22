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

#include "freertos/task.hpp"

constexpr hel::UBaseType kNetworkStackSize = 512U;

constexpr size_t kEspAtBuffSize   = 512;
constexpr size_t kEspAtURCBufSize    = 256;
constexpr size_t kEspAtUrcListSize  = 10;

#endif /* APPLICATION_SETUP_HPP_ */
