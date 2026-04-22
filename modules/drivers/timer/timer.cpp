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
// File-static handle → instance registry
// ---------------------------------------------------------------------------
namespace
{
using namespace hel;

constexpr uint8_t kMaxInstances = 8U;

struct RegistryEntry
{
    TIM_HandleTypeDef* handle   = nullptr;
    Timer*             instance = nullptr;
};

RegistryEntry s_registry[kMaxInstances]{};

void registerInstance(TIM_HandleTypeDef& handle, Timer& timer) noexcept
{
    for (auto& e : s_registry)
    {
        if (e.handle == nullptr)
        {
            e.handle   = &handle;
            e.instance = &timer;
            return;
        }
    }
}

void unregisterInstance(TIM_HandleTypeDef& handle) noexcept
{
    for (auto& e : s_registry)
    {
        if (e.handle == &handle)
        {
            e.handle   = nullptr;
            e.instance = nullptr;
            return;
        }
    }
}

Timer* findInstance(TIM_HandleTypeDef* handle) noexcept
{
    for (const auto& e : s_registry)
    {
        if (e.handle == handle)
        {
            return e.instance;
        }
    }
    return nullptr;
}

TimerStatus translateHalStatus(HAL_StatusTypeDef status) noexcept
{
    switch (status)
    {
        case HAL_OK:   return TimerStatus::Ok;
        case HAL_BUSY: return TimerStatus::Busy;
        default:       return TimerStatus::ErrorGeneral;
    }
}

} // anonymous namespace

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
    registerInstance(handle, *this);
}

Timer::~Timer() noexcept
{
    if (m_active)
    {
        (void)HAL_TIM_Base_Stop_IT(&m_handle);
    }
    unregisterInstance(m_handle);
}

// ---- Timer control --------------------------------------------------------

TimerStatus Timer::start(uint32_t period_us, TimerMode mode) noexcept
{
    if (m_active)
    {
        (void)HAL_TIM_Base_Stop_IT(&m_handle);
        m_active = false;
    }

    // Compute auto-reload value for the desired period.
    // tick_hz = timer_clock / (prescaler + 1)
    // ARR     = tick_hz * period_us / 1_000_000 - 1
    const uint32_t tick_hz = m_tim_clock_hz / (m_handle.Init.Prescaler + 1U);
    const uint32_t arr     = static_cast<uint32_t>(
        static_cast<uint64_t>(tick_hz) * period_us / 1'000'000ULL) - 1U;

    __HAL_TIM_SET_AUTORELOAD(&m_handle, arr);
    m_handle.Init.Period = arr;

    __HAL_TIM_SET_COUNTER(&m_handle, 0U);
    m_overflows = 0U;
    m_mode      = mode;

    const auto status = HAL_TIM_Base_Start_IT(&m_handle);
    if (status != HAL_OK)
    {
        return translateHalStatus(status);
    }

    m_active = true;
    return TimerStatus::Ok;
}

TimerStatus Timer::stop() noexcept
{
    if (!m_active)
    {
        return TimerStatus::NotInitialized;
    }

    (void)HAL_TIM_Base_Stop_IT(&m_handle);
    m_active = false;
    return TimerStatus::Ok;
}

TimerStatus Timer::reset() noexcept
{
    if (!m_active)
    {
        return TimerStatus::NotInitialized;
    }

    __HAL_TIM_SET_COUNTER(&m_handle, 0U);
    m_overflows = 0U;
    return TimerStatus::Ok;
}

// ---- Measurement ----------------------------------------------------------


bool Timer::isActive() const noexcept
{
    return m_active;
}

uint64_t Timer::count() noexcept
{
    if (!m_active)
    {
        return 0ULL;
    }

    const uint32_t arr = __HAL_TIM_GET_AUTORELOAD(&m_handle) + 1U;
    const uint32_t cnt = __HAL_TIM_GET_COUNTER(&m_handle);
    return static_cast<uint64_t>(m_overflows) * arr + cnt;
}

// ---- Callback management --------------------------------------------------

void Timer::setCallback(TimerCallback callback) noexcept
{
    m_callback = callback;
}

// ---- Internal event handler (called from ISR bridge) ----------------------

void Timer::handleEvent()
{
    if (m_mode == TimerMode::OneShot)
    {
        (void)HAL_TIM_Base_Stop_IT(&m_handle);
        m_active = false;
    }
    else
    {
        m_overflows++;
    }

    m_callback();
}

} // namespace hel

// ---------------------------------------------------------------------------
// HAL weak callback override (C linkage)
// ---------------------------------------------------------------------------
extern "C"
{
using namespace hel;

void HEL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef* htim)
{
    Timer* inst = findInstance(htim);
    if (inst != nullptr)
    {
        inst->handleEvent();
    }
}

} // extern "C"
