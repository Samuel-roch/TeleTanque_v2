/**
 ******************************************************************************
 * @file
 * @author  Samuel Almeida Rocha
 * @version
 * @date
 * @ingroup
 * @brief   
 *
 ******************************************************************************
 */

#ifndef DEVICES_ESP_AT_ESP_AT_HPP_
#define DEVICES_ESP_AT_ESP_AT_HPP_

// Helios includes
#include <hel_at_client>
#include <hel_fsm>

// Drivers includes
#include "uart/uart.hpp"
#include "gpio/gpio.hpp"

// FreeRTOS includes
#include "freertos/task.hpp"

#include "esp_basic_cmds.hpp"
#include "esp_wifi_cmds.hpp"

namespace hel
{

constexpr size_t kEsp32StackSize = 256;
constexpr size_t kEspAtBuffSize = 512;
constexpr size_t kEspAtURCBufSize = 256;
constexpr size_t kEspAtUrcListSize = 10;

class EspAt :
  public StaticTask<kEsp32StackSize>, public AtClient<kEspAtBuffSize,
      kEspAtURCBufSize, kEspAtUrcListSize>
{
public:
  EspAt(Uart& handle, Gpio& resetPin);

private: // Constants
  static constexpr std::size_t kFsmTransitionsCapacity = 10U;

private: // Members
  hel::FSM<EspAt, kFsmTransitionsCapacity> m_fsm;
  Gpio &m_resetPin;

  Esp32BasicCmds m_basic_cmds { *this };

  FSMState<EspAt>m_state_init;

  FSMState<EspAt>m_state_idle;
  FSMState<EspAt>m_state_busy;
  FSMState<EspAt>m_state_error;

private: // Methods

  void run() noexcept override;

  void eventReady(String& msg);
  void eventConnect(String& msg);
  void eventTime(String& msg);
  void mqttConnEvets(String& msg);
  void mqttRecvEvent(String& msg);
  void listApEvent(String& msg);

};

} /* namespace dev */

#endif /* DEVICES_ESP_AT_ESP_AT_HPP_ */
