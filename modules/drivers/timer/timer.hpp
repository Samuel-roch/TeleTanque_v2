/**
 ******************************************************************************
 * @file    timer.hpp
 * @author  SENAI CIMATEC (AEE Selene Team)
 * @version 1.0.0
 * @date    Apr 20, 2026
 * @brief   STM32 HAL hardware timer driver.
 ******************************************************************************
 */
#ifndef DRIVERS_TIMER_TIMER_HPP_
#define DRIVERS_TIMER_TIMER_HPP_

#include <hel_target>
#include <hel_itimer>

namespace hel
{

/**
 * @class  Timer
 * @brief  STM32 HAL concrete implementation of @ref iTimer.
 * @details Wraps a CubeMX-initialised TIM peripheral and provides
 *          microsecond-resolution one-shot and periodic timing with an
 *          interrupt-driven callback.
 *
 *          The prescaler is assumed to be fixed (configured by CubeMX).
 *          @ref start recalculates only the auto-reload register (ARR) to
 *          achieve the requested period.
 *
 * @note   One @ref Timer instance per TIM peripheral.
 *         Copy and move are deleted (inherited from @ref iTimer).
 */
class Timer : public iTimer
{
public:

  /**
   * @brief  Construct a Timer driver bound to a HAL TIM handle.
   * @param[in]  handle        Reference to the CubeMX-generated TIM handle.
   *                           Must outlive this object.
   * @param[in]  tim_clock_hz  Timer input clock in Hz after the APB
   *                           multiplier (e.g. 200 000 000 for a 200 MHz
   *                           timer clock on STM32H7).
   */
  Timer(TIM_handle& handle, uint32_t tim_clock_hz) noexcept;

  ~Timer() noexcept override;

  // -------------------------------------------------------------------------
  // Timer control
  // -------------------------------------------------------------------------

  /**
   * @brief  Start (or restart) the timer with the given period.
   * @details Reconfigures the ARR register, resets the counter and overflow
   *          accumulator, then starts the update interrupt.  If the timer is
   *          already running it is stopped first.
   *
   * @param[in]  period_us  Desired period in microseconds.  Must be > 0.
   * @param[in]  mode       @ref TimerMode::OneShot or @ref TimerMode::Periodic.
   * @return @ref TimerStatus::Ok on success.
   * @return @ref TimerStatus::ErrorGeneral on HAL fault.
   */
  [[nodiscard]]
  TimerStatus start(uint32_t period_us, TimerMode mode) noexcept override;

  /**
   * @brief  Stop the timer immediately.
   * @details The counter is frozen at its current value.  No callback fires.
   *
   * @return @ref TimerStatus::Ok if the timer was running and is now stopped.
   * @return @ref TimerStatus::NotInitialized if the timer was already stopped.
   */
  [[nodiscard]]
  TimerStatus stop() noexcept override;

  /**
   * @brief  Reset the counter and overflow accumulator to zero.
   * @details The timer keeps running.  The next callback fires after a full
   *          period from this call.
   *
   * @return @ref TimerStatus::Ok on success.
   * @return @ref TimerStatus::NotInitialized if the timer is not running.
   */
  [[nodiscard]]
  TimerStatus reset() noexcept override;

  // -------------------------------------------------------------------------
  // Measurement
  // -------------------------------------------------------------------------

  /**
   * @brief  Return `true` when the timer is currently counting.
   * @details A @ref TimerMode::OneShot timer becomes inactive automatically
   *          after its first expiry.
   *
   * @return `true` if the timer is running.
   */
  [[nodiscard]]
  bool isActive() const noexcept override;

  /**
   * @brief  Elapsed time since the last @ref start or @ref reset, in microseconds.
   * @details Returns 0 when not active.  In @ref TimerMode::Periodic the
   *          value wraps to zero on each expiry.
   *
   * @return Elapsed time in microseconds.
   */
  [[nodiscard]]
  uint64_t count() noexcept;

  // -------------------------------------------------------------------------
  // Callback
  // -------------------------------------------------------------------------

  /**
   * @brief  Register (or replace) the period-elapsed callback.
   * @param[in]  callback  Callable invoked on each timer expiry.
   *                       Pass a default-constructed @ref TimerCallback to clear.
   */
  void setCallback(TimerCallback callback) noexcept override;

  /**
   * @brief  Internal handler invoked by the HAL ISR bridge.
   * @details In @ref TimerMode::OneShot the timer is stopped before the
   *          user callback fires.
   */
  void handleEvent() override;

private:
  TIM_handle&       m_handle;
  uint32_t          m_tim_clock_hz;
  TimerCallback     m_callback;
  TimerMode         m_mode      = TimerMode::OneShot;
  bool              m_active    = false;
  volatile uint32_t m_overflows = 0U;
};

} // namespace hel

#endif /* DRIVERS_TIMER_TIMER_HPP_ */
