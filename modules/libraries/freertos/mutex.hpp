/**
 ******************************************************************************
 * @file    mutex.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-18
 * @ingroup FREERTOS_MUTEX
 * @brief   FreeRTOS implementation of the iMutex interface.
 *
 * @details
 *   - Backed by a FreeRTOS mutex (@c xSemaphoreCreateMutex) which supports
 *     priority inheritance.
 *   - All operations must be called from task context only (not from ISR).
 *   - The mutex is created in the constructor and deleted in the destructor.
 *
 * @note
 *   A @ref Mutex instance must not outlive the FreeRTOS scheduler. Create
 *   mutexes after @ref kernel::start() has been called, or as global/static
 *   objects initialized before the scheduler starts.
 */

#ifndef MODULES_LIBRARIES_FREERTOS_MUTEX_HPP_
#define MODULES_LIBRARIES_FREERTOS_MUTEX_HPP_

#include <hel_ikernel>

namespace hel
{

/**
 * @brief   FreeRTOS mutex with priority inheritance.
 * @details
 *   - Created via @c xSemaphoreCreateMutex (supports priority inheritance).
 *   - @ref lock() and @ref tryLock() block or poll respectively.
 *   - @ref unlock() must be called by the same task that acquired the lock.
 * @ingroup FREERTOS_MUTEX
 * @note    Not reentrant — do not lock the same mutex twice from the same task.
 */
class Mutex :
  public iMutex
{
public:

  /** @brief Create the FreeRTOS mutex. */
  Mutex() noexcept
  {
    m_handle = xSemaphoreCreateMutex();
  }

  /**
   * @brief   Delete the FreeRTOS mutex.
   * @warning Any tasks blocked on this mutex will be unblocked with an error.
   */
  ~Mutex() noexcept override
  {
    if (m_handle != nullptr)
    {
      vSemaphoreDelete(m_handle);
      m_handle = nullptr;
    }
  }

  // -------------------------------------------------------------------------
  // Lock / unlock  (iMutex)
  // -------------------------------------------------------------------------

  /**
   * @brief     Acquire the mutex, blocking until available or timeout elapses.
   * @param[in] timeout_ms  Maximum wait time in ms. @c UINT32_MAX = indefinite.
   * @return    @ref MutexStatus::Ok      Mutex acquired.
   * @return    @ref MutexStatus::Timeout @p timeout_ms elapsed without acquisition.
   * @return    @ref MutexStatus::Error   Handle is null.
   * @warning   Must not be called from an ISR context.
   */
  [[nodiscard]]
  MutexStatus lock(TickType timeout_ms) noexcept override
  {
    if (m_handle == nullptr)
    {
      return MutexStatus::Error;
    }

    return
        (xSemaphoreTake(m_handle, pdMS_TO_TICKS(timeout_ms)) == pdTRUE) ?
            MutexStatus::Ok : MutexStatus::Timeout;
  }

  /**
   * @brief   Attempt to acquire the mutex without blocking.
   * @details Equivalent to @ref lock() with a timeout of zero.
   * @return  @ref MutexStatus::Ok      Mutex acquired immediately.
   * @return  @ref MutexStatus::Timeout Mutex is currently held.
   * @return  @ref MutexStatus::Error   Handle is null.
   */
  [[nodiscard]]
  MutexStatus tryLock() noexcept override
  {
    if (m_handle == nullptr)
    {
      return MutexStatus::Error;
    }
    return
        (xSemaphoreTake(m_handle, 0) == pdTRUE) ?
            MutexStatus::Ok : MutexStatus::Timeout;
  }

  /**
   * @brief   Release the mutex.
   * @return  @ref MutexStatus::Ok    Mutex released.
   * @return  @ref MutexStatus::Error Release failed (e.g., not the owner).
   * @warning Must be called by the same task that acquired the mutex.
   */
  [[nodiscard]]
  MutexStatus unlock() noexcept override
  {
    if (m_handle == nullptr)
    {
      return MutexStatus::Error;
    }
    return
        (xSemaphoreGive(m_handle) == pdTRUE) ?
            MutexStatus::Ok : MutexStatus::Error;
  }

private:



  SemHandle m_handle { nullptr }; ///< FreeRTOS semaphore handle (mutex type).
};

} // namespace hel

#endif // MODULES_LIBRARIES_FREERTOS_MUTEX_HPP_
