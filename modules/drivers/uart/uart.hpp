/**
 ******************************************************************************
 * @file    uart.hpp
 * @author
 * @version
 * @date    Apr 19, 2026
 * @brief   
 *
 */
#ifndef DRIVERS_UART_UART_HPP_
#define DRIVERS_UART_UART_HPP_

#include <hel_target>
#include <hel_iuart>


namespace hel
{

class Uart : public iUart
{
public:
  Uart(UART_handle& handle) noexcept;
  ~Uart() noexcept override;

  // -------------------------------------------------------------------------
  // Blocking transfers
  // -------------------------------------------------------------------------

  /**
   * @brief  Transmit data and block until complete or timeout.
   * @param[in]  data        View of bytes to transmit.
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return @ref ReturnCode::kOk on success.
   * @return @ref ReturnCode::kTimeout if the deadline elapsed.
   * @return @ref ReturnCode::kBusy if a transfer is already active.
   * @return @ref ReturnCode::kErrorHardware on peripheral fault.
   */
  [[nodiscard]]
  ReturnCode write(ConstByteArray& data, uint32_t timeout_ms) noexcept override;

  /**
   * @brief  Receive exactly `data.size()` bytes, blocking until done or timeout.
   * @param[out] data        Writable view to store received bytes.
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return @ref ReturnCode::kOk on success.
   * @return @ref ReturnCode::kTimeout if the deadline elapsed.
   * @return @ref ReturnCode::kBusy if a transfer is already active.
   * @return @ref ReturnCode::kErrorHardware on peripheral fault.
   */
  [[nodiscard]]
   ReturnCode read(ByteArray& data, uint32_t timeout_ms) noexcept override;

  // -------------------------------------------------------------------------
  // Asynchronous transfers
  // -------------------------------------------------------------------------

  /**
   * @brief  Transmit data asynchronously using interrupts or DMA.
   * @param[in]  data    View of bytes to transmit.
   * @param[in]  mode    Transfer mode (normal, to-idle, circular).
   * @return @ref ReturnCode::kOk if the transfer was started successfully.
   * @return @ref ReturnCode::kBusy if a transfer is already active.
   * @return @ref ReturnCode::kErrorHardware on peripheral fault.
   */
  [[nodiscard]]
   ReturnCode writeInterrupt(ConstByteArray& data, UartMode mode) noexcept override;

  /**
   * @brief  Receive data asynchronously using interrupts or DMA.
   * @param[out] data    Writable view to store received bytes.
   * @param[in]  mode    Transfer mode (normal, to-idle, circular).
   * @return @ref ReturnCode::kOk if the transfer was started successfully.
   * @return @ref ReturnCode::kBusy if a transfer is already active.
   * @return @ref ReturnCode::kErrorHardware on peripheral fault.
   */
  [[nodiscard]]
   ReturnCode readInterrupt(ByteArray& data, UartMode mode) noexcept override;

  /**
   * @brief  Transmit data asynchronously using DMA.
   * @param[in]  data    View of bytes to transmit.
   * @param[in]  mode    Transfer mode (normal, to-idle, circular).
   * @return @ref ReturnCode::kOk if the transfer was started successfully.
   * @return @ref ReturnCode::kBusy if a transfer is already active.
   * @return @ref ReturnCode::kErrorHardware on peripheral fault.
   */
  [[nodiscard]]
  ReturnCode writeDMA(ConstByteArray& data, UartMode mode) noexcept override;

  /**
   * @brief  Receive data asynchronously using DMA.
   * @param[out] data    Writable view to store received bytes.
   * @param[in]  mode    Transfer mode (normal, to-idle, circular).
   * @return @ref ReturnCode::kOk if the transfer was started successfully.
   * @return @ref ReturnCode::kBusy if a transfer is already active.
   * @return @ref ReturnCode::kErrorHardware on peripheral fault.
   */
  [[nodiscard]]
  ReturnCode readDMA(ByteArray& data, UartMode mode) noexcept override;

  // -------------------------------------------------------------------------
  // Abort
  // -------------------------------------------------------------------------

  /**
   * @brief  Abort an ongoing asynchronous transmission.
   * @details Triggers @ref UartEvent::kAbortComplete (or @ref UartEvent::kAbortError)
   *          via the registered callback after the peripheral stops.
   * @return @ref ReturnCode::kOk if abort was issued.
   * @return @ref ReturnCode::kNotInitialized if no transfer is active.
   */
  [[nodiscard]]
  ReturnCode abortWrite() noexcept override;

  /**
   * @brief  Abort an ongoing asynchronous reception.
   * @details Triggers @ref UartEvent::kAbortComplete (or @ref UartEvent::kAbortError)
   *          via the registered callback after the peripheral stops.
   * @return @ref ReturnCode::kOk if abort was issued.
   * @return @ref ReturnCode::kNotInitialized if no transfer is active.
   */
  [[nodiscard]]
  ReturnCode abortRead() noexcept override;

  // -------------------------------------------------------------------------
  // Callback and state
  // -------------------------------------------------------------------------

  /**
   * @brief  Register (or replace) the asynchronous event callback.
   *
   * @param[in]  callback  Callable invoked on every @ref UartEvent.
   *                       Pass a default-constructed @ref UartCallback to clear.
   */
  void setCallback(UartCallback callback) noexcept override;

  /**
   * @brief  Internal handler for UART events, called by the driver/ISR.
   *
   * @param[in]  event   The event that occurred.
   * @param[in]  count   Bytes transferred (0 for error/abort events).
   */
  void handleEvent(UartEvent code, uint16_t count);

private:
  UART_handle &m_handle;    // Underlying HAL UART handle
  UartCallback m_callback;

};

} /* namespace hel */

#endif /* DRIVERS_UART_UART_HPP_ */
