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

#include "esp_at.hpp"

namespace hel
{

EspAt::EspAt(UART_handle& handle, Gpio& resetPin)
    :
    StaticTask("Esp32", TaskPriority::Normal),
    Uart(handle),
    m_fsm(this),
    m_resetPin(resetPin)
{
  m_urc_entries[0] = { "ready", &EspAt::onReady };
  m_urc_entries[1] = { "WIFI CONNECTED", &EspAt::onConnected };
  m_urc_entries[2] = { "WIFI GOT IP", &EspAt::onTimeUpdated };
  m_urc_entries[3] = { "+MQTTCONNECTED", &EspAt::onMqttConn };
  m_urc_entries[4] = { "+MQTTRECV", &EspAt::onMqttRecvEvent };
  m_urc_entries[5] = { "+CWLAP:", &EspAt::onListAp };
  m_urc_count = 6U;

  m_state_init.set(&EspAt::onStateInit, &EspAt::enterStateInit);
  m_state_idle.set(&EspAt::onStateIdle);
  m_state_busy.set(&EspAt::onStateBusy, &EspAt::enterStateBusy);
  m_state_error.set(&EspAt::onStateError);

  m_fsm.addTransition(&m_state_init, &m_state_idle, TriggerReady);
  m_fsm.addTransition(&m_state_init, &m_state_error, TriggerError);
  m_fsm.addTransition(&m_state_init, &m_state_busy, TriggerCommand);
  m_fsm.addTransition(&m_state_idle, &m_state_busy, TriggerCommand);
  m_fsm.addTransition(&m_state_idle, &m_state_error, TriggerError);
  m_fsm.addTransition(&m_state_busy, &m_state_idle, TriggerSuccess);

  m_fsm.setInitialState(&m_state_init);

  m_ready = false;
  m_connected = false;
}

void EspAt::run() noexcept
{
  (void)TaskBase::notifyTake();

  for ( ;; )
  {
    processURC();
    m_fsm.runMachine();
    hel::sleep(10);
  }
}

void EspAt::reset() noexcept
{
  m_ready = false;
  m_connected = false;

  (void)Uart::readDMA(m_rx_string, UartMode::ToIdle);

  m_resetPin.write(false);
  hel::sleep(100);
  m_resetPin.write(true);
  hel::sleep(100);

}

// -------------------------------------------------------------------------
// Private methods
// -------------------------------------------------------------------------

/**
 * @brief   Dispatch the oldest pending URC to its registered callback.
 * @details
 *   - Must be called regularly from task context (e.g. main loop or a
 *     periodic task) to drain the URC mailbox.
 *   - Processes one URC per call; call repeatedly until no more URCs are
 *     pending if low latency is required.
 *   - Strips leading whitespace (`' '`, `'\t'`, `'\r'`, `'\n'`) before
 *     matching against registered keys.
 *   - Unmatched URCs are silently discarded.
 *
 * @note  Must not be called from ISR context. Uses `__disable_irq` /
 *        `__enable_irq` (Cortex-M) for brief critical sections to safely
 *        access @ref m_urc_buffer which is also written from ISR context.
 */
void EspAt::processURC() noexcept
{
  // Check quickly under a critical section whether there is anything to do.

  if (m_urc_buffer.receive(m_temp_string, 10) != QueueStatus::Ok)
  {
    return;
  }

  if (m_temp_string.empty())
  {
    return;
  }

  if (m_temp_string.trim(" \t\r\n") == false)
  {
    return;
  }

  // Dispatch to the first matching callback.
  for ( std::size_t i = 0U; i < m_urc_count; ++i )
  {
    if (std::strstr(m_temp_string.c_str(), m_urc_entries[i].key) != nullptr)
    {
      (this->*(m_urc_entries[i].callback))(m_temp_string);
      return;
    }
  }
}

/**
 * @brief   UART event handler registered as the @ref UartCallback.
 * @details Called from ISR context by the UART driver. On
 *          @ref UartEvent::RxComplete:
 *            1. Syncs @ref m_rx_string size after the DMA write.
 *            2. Ignores bare `"\r\n"` frames (keep-alive / echo artefacts).
 *            3. If @ref waitForResponse is pending, signals it by clearing
 *               @ref m_waiting_for_response.
 *            4. Otherwise stores the frame in the URC mailbox.
 *            5. Re-arms DMA for the next frame.
 *          All other events are ignored.
 *
 * @param[in] event  UART event type.
 * @param[in] count  Bytes transferred (unused; length derived from @ref sync).
 *
 * @warning  Called from ISR context. Must not call any blocking function.
 */
void EspAt::handleEvent(UartEvent event, uint16_t count) noexcept
{
  (void) count;

  if (event != UartEvent::RxComplete)
  {
    (void)Uart::readDMA(m_rx_string, UartMode::ToIdle);
    return;
  }

  m_rx_string.sync();

  const bool is_bare_crlf = (m_rx_string.size() == 2U)
      && (m_rx_string.data()[0] == '\r') && (m_rx_string.data()[1] == '\n');

  if (!is_bare_crlf)
  {
    if (m_waiting_for_response)
    {
      // Unblock waitForResponse — it will read m_rx_string directly.
      m_waiting_for_response = false;
    }
    else
    {
      // Queue as URC; processURC() will consume it from task context.
      (void) m_urc_buffer.sendToBackFromIsr(m_rx_string);
    }
  }

  // Re-arm DMA for the next variable-length frame.
  (void) Uart::readDMA(m_rx_string, UartMode::ToIdle);
}

/**
 * @brief   Append `"\r\n"` to the TX buffer, send it, and wait for a response.
 * @details
 *   - The TX buffer must be pre-filled by the caller via @ref txBuffer before
 *     calling this method (e.g. `at.txBuffer() << "AT+RST"`).
 *   - Appends `"\r\n"` to the TX buffer, then transmits the whole buffer.
 *   - Waits for the next received frame and checks whether it contains
 *     @p expected_response as a sub-string.
 *   - Clears the TX buffer after sending regardless of outcome.
 *
 * @param[in] expected_response  Null-terminated string to look for in the
 *                               reply; must not be `nullptr`.
 * @param[in] timeout_ms         Maximum wait time in milliseconds.
 *                               Defaults to @ref kDefaultTimeoutMs.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : Response contained @p expected_response.
 *   - @ref ReturnCode::ErrorGeneral      : TX buffer was empty or too full for `"\r\n"`.
 *   - @ref ReturnCode::ErrorGeneral      : UART write failed.
 *   - @ref ReturnCode::ErrorInvalidState : Frame received but did not match.
 *   - @ref ReturnCode::ErrorTimeout      : No response within @p timeout_ms.
 *
 * @note  Must not be called from ISR context.
 */
ReturnCode EspAt::execCommand(
  const char* expected_response, uint32_t timeout_ms) noexcept
{
  if (m_temp_string.empty())
  {
    return ReturnCode::ErrorGeneral;
  }
  if (m_temp_string.remaining() < 2U)
  {
    return ReturnCode::ErrorGeneral;
  }

  m_rx_string.clear();
  m_temp_string << "\r\n";

  ByteArray tx_view(m_temp_string);
  const ReturnCode rc = Uart::write(tx_view, kDefaultTimeoutMs);
  m_temp_string.clear();

  if (rc != ReturnCode::AnsweredRequest)
  {
    return rc;
  }

  return waitForResponse(expected_response, timeout_ms);
}

/**
 * @brief   Busy-wait until the expected response arrives or timeout elapses.
 * @details @p m_waiting_for_response is set here and cleared by
 *          @ref onUartEvent from ISR context when a new frame is received.
 *
 * @param[in] expected_response  Sub-string to look for in @ref m_rx_string.
 * @param[in] timeout_ms         Maximum wait time in milliseconds.
 * @return  ReturnCode
 *   - @ref ReturnCode::AnsweredRequest   : RX buffer contained @p expected_response.
 *   - @ref ReturnCode::ErrorInvalidState : Frame received but no match.
 *   - @ref ReturnCode::ErrorTimeout      : Timeout elapsed before any frame arrived.
 *
 * @note  Busy-waits the CPU. In an RTOS environment replace this loop with a
 *        semaphore or event-flag pend to allow other tasks to be scheduled.
 */
ReturnCode EspAt::waitForResponse(
  const char* expected_response, uint32_t timeout_ms) noexcept
{
  m_cmd_timeout.start(timeout_ms);
  m_waiting_for_response = true;

  do
  {
    if (!m_waiting_for_response)
    {
      return
          m_rx_string.contains(expected_response) ?
              ReturnCode::AnsweredRequest : ReturnCode::ErrorInvalidState;
    }
  }
  while (!m_cmd_timeout.expired());

  // Timeout — cancel the pending wait flag.
  m_waiting_for_response = false;
  return ReturnCode::ErrorTimeout;
}

// Event Rady: Triggered when the ESP32 signals it is ready after reset.
void EspAt::onReady(String& msg)
{
  if(msg.contains("ready"))
  {
    m_ready = true;
  }
}

// Event Connect: Triggered on Wi-Fi connection status changes (e.g., "WIFI CONNECTED").
void EspAt::onConnected(String& msg)
{

}

// Event Time: Triggered when the ESP32 reports the current time (e.g., "TIME: ...").
void EspAt::onTimeUpdated(String& msg)
{

}

// Event MqttConnected: Triggered when the ESP32 reports MQTT connection status (e.g., "MQTT CONNECTED").
void EspAt::onMqttConn(String& msg)
{

}

// Event MqttRecv: Triggered when the ESP32 receives an MQTT message (e.g., "MQTT RECV: ...").
void EspAt::onMqttRecvEvent(String& msg)
{

}

// Event ListAp: Triggered when the ESP32 reports available Wi-Fi networks (e.g., "AP: ...").
void EspAt::onListAp(String& msg)
{

}

// FSM state handlers

//State Init: Initial state after reset, waiting for "ready" event.
void EspAt::enterStateInit()
{
  reset();
  m_cmd_timeout.start(kDefaultTimeoutMs);
}

void EspAt::onStateInit()
{
  if (m_ready)
  {
    // Transition to Idle state on ready event.
    m_fsm.trigger(TriggerReady);
  }
  else if (m_cmd_timeout.expired())
  {
    // Transition to Error state on timeout.
    m_fsm.trigger(TriggerError);
  }
}

void EspAt::onStateIdle()
{

}

void EspAt::enterStateBusy()
{

}

void EspAt::onStateBusy()
{

}

//State Error: State entered on error conditions, may attempt recovery or wait for reset.
void EspAt::onStateError()
{

}

} /* namespace dev */
