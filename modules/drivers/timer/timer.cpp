/**
 ******************************************************************************
 * @file    timer.cpp
 * @author  SENAI CIMATEC (AEE Selene Team)
 * @version 1.0.0
 * @date    Apr 20, 2026
 * @brief   STM32 HAL hardware timer driver implementation.
 ******************************************************************************
 */
#include <timer/timer.hpp>


// ---------------------------------------------------------------------------
// hel::Timer implementation
// ---------------------------------------------------------------------------
namespace hel
{

Timer::Timer(TIM_handle& handle, uint32_t tim_clock_hz) noexcept
    :
    m_handle(handle),
    m_tim_clock_hz(tim_clock_hz)
{
}

Timer::~Timer() noexcept
{

}


// ---- Timer control --------------------------------------------------------

ReturnCode Timer::start(uint32_t period_us, TimerMode mode) noexcept
{
  (void)period_us;
  (void)mode;

  return ReturnCode::AnsweredRequest;
}

ReturnCode Timer::stop() noexcept
{

  return ReturnCode::AnsweredRequest;
}

ReturnCode Timer::reset() noexcept
{

  return ReturnCode::AnsweredRequest;
}

// ---- Measurement ----------------------------------------------------------


bool Timer::isActive() const noexcept
{
    return m_active;
}

uint64_t Timer::count() noexcept
{

    return 0U;
}

// ---- Callback management --------------------------------------------------

void Timer::setCallback(TimerCallback callback) noexcept
{
    m_callback = callback;
}

// ---- Internal event handler (called from ISR bridge) ----------------------

void Timer::handleEvent()
{

}

} // namespace hel

// ---------------------------------------------------------------------------
// HAL weak callback override (C linkage)
// ---------------------------------------------------------------------------
extern "C"
{
} // extern "C"
