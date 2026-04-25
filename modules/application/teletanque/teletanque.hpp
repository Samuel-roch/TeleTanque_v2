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
#ifndef APPLICATION_TELETANQUE_TELETANQUE_HPP_
#define APPLICATION_TELETANQUE_TELETANQUE_HPP_

#include <hel_target>
#include "network/network.hpp"
#include "setup.hpp"

class Teletanque :
  public hel::StaticTask<kTeletanqueStackSize>
{
public:

  static Teletanque& instance() noexcept
  {
    static Teletanque instance;
    return instance;
  }

private:  // Methods
  explicit Teletanque() noexcept;
  void run() noexcept override;

private: // Members
  Network& m_network = Network::instance();

};

#endif /* APPLICATION_TELETANQUE_TELETANQUE_HPP_ */
