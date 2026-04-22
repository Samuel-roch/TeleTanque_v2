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

private:
  explicit Network() noexcept;
  void run() noexcept override;

};

#endif /* APPLICATION_NETWORK_NETWORK_HPP_ */
