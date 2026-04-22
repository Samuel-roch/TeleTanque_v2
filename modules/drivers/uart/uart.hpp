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
   * @return @ref UartStatus::kOk on success.
   * @return @ref UartStatus::kTimeout if the deadline elapsed.
   * @return @ref UartStatus::kBusy if a transfer is already active.
   * @return @ref UartStatus::kErrorHardware on peripheral fault.
   */
  [[nodiscard]]
  UartStatus write(ConstByteArray& data, uint32_t timeout_ms) noexcept;

  /**
   * @brief  Receive exactly `data.size()` bytes, blocking until done or timeout.
   * @param[out] data        Writable view to store received bytes.
   * @param[in]  timeout_ms  Maximum wait time in milliseconds.
   * @return @ref UartStatus::kOk on success.
   * @return @ref UartStatus::kTimeout if the deadline elapsed.
   * @return @ref UartStatus::kBusy if a transfer is already active.
   * @return @ref UartStatus::kErrorHardware on peripheral fault.
   */
  [[nodiscard]]
  UartStatus read(ByteArray& data, uint32_t timeout_ms) noexcept;

  // -------------------------------------------------------------------------
  // Asynchronous transfers
  // -------------------------------------------------------------------------

  /**
   * @brief  Transmit data asynchronously using interrupts or DMA.
   * @param[in]  data    View of bytes to transmit.
   * @param[in]  mode    Transfer mode (normal, to-idle, circular).
   * @return @ref UartStatus::kOk if the transfer was started successfully.
   * @return @ref UartStatus::kBusy if a transfer is already active.
   * @return @ref UartStatus::kErrorHardware on peripheral fault.
   */
  [[nodiscard]]
  UartStatus writeInterrupt(ConstByteArray& data, SerialMode mode) noexcept;

  /**
   * @brief  Receive data asynchronously using interrupts or DMA.
   * @param[out] data    Writable view to store received bytes.
   * @param[in]  mode    Transfer mode (normal, to-idle, circular).
   * @return @ref UartStatus::kOk if the transfer was started successfully.
   * @return @ref UartStatus::kBusy if a transfer is already active.
   * @return @ref UartStatus::kErrorHardware on peripheral fault.
   */
  [[nodiscard]]
  UartStatus readInterrupt(ByteArray& data, SerialMode mode) noexcept;

  /**
   * @brief  Transmit data asynchronously using DMA.
   * @param[in]  data    View of bytes to transmit.
   * @param[in]  mode    Transfer mode (normal, to-idle, circular).
   * @return @ref UartStatus::kOk if the transfer was started successfully.
   * @return @ref UartStatus::kBusy if a transfer is already active.
   * @return @ref UartStatus::kErrorHardware on peripheral fault.
   */
  [[nodiscard]]
  UartStatus writeDMA(ConstByteArray& data, SerialMode mode) noexcept;

  /**
   * @brief  Receive data asynchronously using DMA.
   * @param[out] data    Writable view to store received bytes.
   * @param[in]  mode    Transfer mode (normal, to-idle, circular).
   * @return @ref UartStatus::kOk if the transfer was started successfully.
   * @return @ref UartStatus::kBusy if a transfer is already active.
   * @return @ref UartStatus::kErrorHardware on peripheral fault.
   */
  [[nodiscard]]
  UartStatus readDMA(ByteArray& data, SerialMode mode) noexcept;

  // -------------------------------------------------------------------------
  // Abort
  // -------------------------------------------------------------------------

  /**
   * @brief  Abort an ongoing asynchronous transmission.
   * @details Triggers @ref UartEvent::kAbortComplete (or @ref UartEvent::kAbortError)
   *          via the registered callback after the peripheral stops.
   * @return @ref UartStatus::kOk if abort was issued.
   * @return @ref UartStatus::kNotInitialized if no transfer is active.
   */
  [[nodiscard]]
  UartStatus abortWrite() noexcept;

  /**
   * @brief  Abort an ongoing asynchronous reception.
   * @details Triggers @ref UartEvent::kAbortComplete (or @ref UartEvent::kAbortError)
   *          via the registered callback after the peripheral stops.
   * @return @ref UartStatus::kOk if abort was issued.
   * @return @ref UartStatus::kNotInitialized if no transfer is active.
   */
  [[nodiscard]]
  UartStatus abortRead() noexcept;

  // -------------------------------------------------------------------------
  // Callback and state
  // -------------------------------------------------------------------------

  /**
   * @brief  Register (or replace) the asynchronous event callback.
   *
   * @param[in]  callback  Callable invoked on every @ref UartEvent.
   *                       Pass a default-constructed @ref UartCallback to clear.
   */
  void setCallback(UartCallback callback) noexcept;

  /**
   * @brief  Internal handler for UART events, called by the driver/ISR.
   *
   * @param[in]  event   The event that occurred.
   * @param[in]  count   Bytes transferred (0 for error/abort events).
   */
  void handleEvent(UartStatus event, uint16_t count);

private:
  UART_handle &m_handle;    // Underlying HAL UART handle
  UartCallback m_callback;

};

} /* namespace hel */

#endif /* DRIVERS_UART_UART_HPP_ */
