/**
 ******************************************************************************
 * @file    semaphore.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-18
 * @ingroup FREERTOS_SEMAPHORE
 * @brief   FreeRTOS implementation of the iSemaphore interface.
 *
 * @details
 *   - Backed by @c xSemaphoreCreateCounting, supporting both binary and
 *     counting semaphore semantics.
 *   - Binary semaphore: @c Semaphore(1u, 0u) (default).
 *   - Counting semaphore: @c Semaphore(maxCount, initialCount).
 *   - @ref give() and @ref giveFromIsr() are safe from task and ISR contexts
 *     respectively. @ref take() must be called from task context only.
 *
 * @note
 *   The semaphore is created in the constructor. Ensure the FreeRTOS heap
 *   has sufficient space when using @c Semaphore as a dynamic object.
 */

#ifndef MODULES_LIBRARIES_FREERTOS_SEMAPHORE_HPP_
#define MODULES_LIBRARIES_FREERTOS_SEMAPHORE_HPP_

#include <hel_ikernel>

namespace hel
{

/**
 * @brief   FreeRTOS counting / binary semaphore.
 * @details
 *   - For a binary semaphore: `Semaphore(1u, 0u)`.
 *   - For a counting semaphore: `Semaphore(N, 0u)`.
 *   - @ref giveFromIsr() performs a @c portYIELD_FROM_ISR if a higher-priority
 *     task was unblocked.
 * @ingroup FREERTOS_SEMAPHORE
 */
class Semaphore :
  public iSemaphore
{
public:

  /**
   * @brief     Construct and create a FreeRTOS counting semaphore.
   * @param[in] maxCount      Maximum token count (1 = binary semaphore).
   * @param[in] initialCount  Initial token count (0 = taken at construction).
   */
  explicit Semaphore(
    UBaseType maxCount = 1u, UBaseType initialCount = 0u) noexcept
  {
    m_handle = xSemaphoreCreateCounting(maxCount, initialCount);
  }

  /**
   * @brief   Delete the FreeRTOS semaphore.
   * @warning Any tasks blocked on this semaphore will be unblocked with an error.
   */
  ~Semaphore() noexcept override
  {
    if (m_handle != nullptr)
    {
      vSemaphoreDelete(m_handle);
      m_handle = nullptr;
    }
  }

  // -------------------------------------------------------------------------
  // Give (signal)  (iSemaphore)
  // -------------------------------------------------------------------------

  /**
   * @brief   Increment the semaphore count from task context.
   * @return  @ref SemaphoreStatus::Ok   Token given; highest-priority waiter unblocked.
   * @return  @ref SemaphoreStatus::Full Count already at maximum.
   * @return  @ref SemaphoreStatus::Error Handle is null.
   */
  [[nodiscard]]
  SemaphoreStatus give() noexcept override
  {
    if (m_handle == nullptr)
    {
      return SemaphoreStatus::Error;
    }
    return
        (xSemaphoreGive(m_handle) == pdTRUE) ?
            SemaphoreStatus::Ok : SemaphoreStatus::Full;
  }

  /**
   * @brief   Increment the semaphore count from ISR context.
   * @details Performs @c portYIELD_FROM_ISR if a higher-priority task woke up.
   * @return  @ref SemaphoreStatus::Ok   Token given.
   * @return  @ref SemaphoreStatus::Full Count already at maximum.
   * @return  @ref SemaphoreStatus::Error Handle is null.
   */
  [[nodiscard]]
  SemaphoreStatus giveFromIsr() noexcept override
  {
    if (m_handle == nullptr)
    {
      return SemaphoreStatus::Error;
    }
    BaseType xHPTW = pdFALSE;
    const BaseType rc = xSemaphoreGiveFromISR(m_handle, &xHPTW);
    portYIELD_FROM_ISR(xHPTW);
    return (rc == pdTRUE) ? SemaphoreStatus::Ok : SemaphoreStatus::Full;
  }

  // -------------------------------------------------------------------------
  // Take (wait)  (iSemaphore)
  // -------------------------------------------------------------------------

  /**
   * @brief     Decrement the semaphore count, blocking until a token is available.
   * @param[in] timeout_ms  Maximum wait time in ms. @c UINT32_MAX = indefinite.
   * @return    @ref SemaphoreStatus::Ok      Token taken.
   * @return    @ref SemaphoreStatus::Timeout Timeout elapsed before a token appeared.
   * @return    @ref SemaphoreStatus::Error   Handle is null.
   * @warning   Must not be called from an ISR context.
   */
  [[nodiscard]]
  SemaphoreStatus take(TickType timeout_ms) noexcept override
  {
    if (m_handle == nullptr)
    {
      return SemaphoreStatus::Error;
    }

    return
        (xSemaphoreTake(m_handle, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) ?
            SemaphoreStatus::Ok : SemaphoreStatus::Timeout;
  }

  // -------------------------------------------------------------------------
  // State query  (iSemaphore)
  // -------------------------------------------------------------------------

  /**
   * @brief   Return the current semaphore token count.
   * @return  Number of available tokens; 0 if the handle is null.
   */
  [[nodiscard]]
   UBaseType count() const noexcept override
  {
    if (m_handle == nullptr)
    {
      return 0u;
    }
    return uxSemaphoreGetCount(m_handle);
  }

private:

  SemHandle m_handle { nullptr }; ///< FreeRTOS semaphore handle.
};

} // namespace hel

#endif // MODULES_LIBRARIES_FREERTOS_SEMAPHORE_HPP_
