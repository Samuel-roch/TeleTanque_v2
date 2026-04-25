/**
 ******************************************************************************
 * @file    kernel.cpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-18
 * @ingroup FREERTOS_KERNEL
 * @brief   FreeRTOS implementation of the iKernel free-function interface.
 *
 * @details
 *   - Implements @ref kernel::start(), @ref kernel::stop(), @ref kernel::delay(),
 *     @ref kernel::yield(), @ref kernel::getTickMs(), @ref kernel::getTickMsFromIsr(),
 *     and the critical-section pair @ref kernel::enterCritical() /
 *     @ref kernel::exitCritical() (plus ISR variants).
 *   - @ref start() uses an internal flag to guard against multiple calls;
 *     the FreeRTOS scheduler does not return on success.
 *   - @ref stop() calls @c vTaskEndScheduler(); availability depends on the
 *     FreeRTOS port (supported on ARM7/ARM9; limited on Cortex-M).
 *
 * @note
 *   All functions except those explicitly marked @c FromIsr must be called
 *   from task context only.
 */

#include <hel_ikernel>
#include "FreeRTOS.h"

namespace hel
{

// -------------------------------------------------------------------------
// Scheduler lifecycle
// -------------------------------------------------------------------------

/**
 * @brief   Start the FreeRTOS scheduler.
 * @details This function does not return if the scheduler starts successfully.
 *          It returns only when the heap is too small to create the idle or
 *          timer tasks, in which case @ref KernelStatus::Error is returned.
 * @return  @ref KernelStatus::AlreadyStarted Scheduler was already started.
 * @return  @ref KernelStatus::Error          Insufficient heap for FreeRTOS internals.
 */
[[nodiscard]]
KernelStatus start() noexcept
{
    static bool s_started = false;
    if (s_started)
    {
        return KernelStatus::AlreadyStarted;
    }
    s_started = true;
    vTaskStartScheduler();
    return KernelStatus::Ok;
}

/**
 * @brief   Stop the FreeRTOS scheduler.
 * @details Calls @c vTaskEndScheduler(). Availability depends on the FreeRTOS
 *          port; not all Cortex-M ports implement this function.
 * @return  @ref KernelStatus::NotStarted Scheduler has not been started.
 * @return  @ref KernelStatus::Ok         Scheduler stopped.
 */
[[nodiscard]]
KernelStatus stop() noexcept
{
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED)
    {
        return KernelStatus::NotStarted;
    }
    vTaskEndScheduler();
    return KernelStatus::Ok;
}

// -------------------------------------------------------------------------
// Task control
// -------------------------------------------------------------------------

/**
 * @brief     Block the current task for at least @p ms milliseconds.
 * @param[in] ms  Duration in milliseconds. Zero yields without sleeping.
 * @note      Must not be called from an ISR context.
 */
void sleep(TickType ms) noexcept
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

/**
 * @brief   Yield the current task to allow other ready tasks to run.
 * @note    Must not be called from an ISR context.
 */
void yield() noexcept
{
    taskYIELD();
}

// -------------------------------------------------------------------------
// System tick
// -------------------------------------------------------------------------

/**
 * @brief   Return the system tick counter converted to milliseconds.
 * @details The counter starts at zero when the scheduler starts and wraps
 *          after @c UINT32_MAX ms (~49 days at 1 ms resolution).
 * @return  Current tick count in milliseconds.
 */
[[nodiscard]]
TickType getTickMs() noexcept
{
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

/**
 * @brief   Return the system tick counter in milliseconds from ISR context.
 * @return  Current tick count in milliseconds.
 */
[[nodiscard]]
TickType getTickMsFromIsr() noexcept
{
    return xTaskGetTickCountFromISR() * portTICK_PERIOD_MS;
}

// -------------------------------------------------------------------------
// Critical sections
// -------------------------------------------------------------------------

/**
 * @brief   Enter a critical section, disabling task preemption and interrupts.
 * @details Must be paired with a matching @ref exitCritical() call.
 *          Keep critical sections as short as possible.
 * @warning Must not be called from an ISR context.
 */
void enterCritical() noexcept
{
    taskENTER_CRITICAL();
}

/**
 * @brief   Exit a critical section, re-enabling preemption and interrupts.
 * @details Must be paired with a preceding @ref enterCritical() call.
 */
void exitCritical() noexcept
{
    taskEXIT_CRITICAL();
}

/**
 * @brief   Enter a critical section from an ISR context.
 * @details Saves and returns the previous interrupt mask.
 * @return  Saved interrupt mask; pass to @ref exitCriticalFromIsr().
 */
[[nodiscard]]
 UBaseType enterCriticalFromIsr() noexcept
{
    return taskENTER_CRITICAL_FROM_ISR();
}

/**
 * @brief     Exit a critical section from an ISR context.
 * @param[in] savedMask  Interrupt mask returned by @ref enterCriticalFromIsr().
 */
void exitCriticalFromIsr(uint32_t savedMask) noexcept
{
    taskEXIT_CRITICAL_FROM_ISR(savedMask);
}

} // namespace hel
