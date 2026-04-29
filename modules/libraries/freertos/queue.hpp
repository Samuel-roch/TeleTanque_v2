/**
 ******************************************************************************
 * @file    queue.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-18
 * @ingroup FREERTOS_QUEUE
 * @brief   FreeRTOS implementation of the iQueue interface.
 *
 * @details
 *   - @ref QueueBase<T> implements all @ref iQueue<T> methods; the FreeRTOS
 *     handle and capacity are protected members accessible to derived classes.
 *   - @ref DynamicQueue<T> creates the queue on the heap via @c xQueueCreate.
 *   - @ref StaticQueue<T,Capacity> uses @c xQueueCreateStatic with no heap.
 *
 * @note
 *   Item type @c T must be trivially copyable. FreeRTOS copies items by value
 *   using @c memcpy internally.
 */

#ifndef MODULES_LIBRARIES_FREERTOS_QUEUE_HPP_
#define MODULES_LIBRARIES_FREERTOS_QUEUE_HPP_

#include <hel_ikernel>

namespace hel
{

/**
 * @brief   Abstract FreeRTOS queue base — all send/receive/query logic.
 * @details
 *   - @c m_handle and @c m_capacity are protected so that @ref DynamicQueue
 *     and @ref StaticQueue can initialize them without bypassing the interface.
 *   - All task-context operations accept a @p timeout_ms of @c UINT32_MAX
 *     to block indefinitely.
 * @tparam  T  Item type (must be trivially copyable).
 * @ingroup FREERTOS_QUEUE
 * @note    Methods ending in @c FromISR are safe to call from interrupt context.
 *          All other methods must be called from task context only.
 */
template<typename T>
class QueueBase :
  public iQueue<T>
{
public:

  // -------------------------------------------------------------------------
  // Send
  // -------------------------------------------------------------------------

  /**
   * @brief     Post an item to the back of the queue from task context.
   * @param[in] item        Item to copy into the queue.
   * @param[in] timeout_ms  Maximum wait time if the queue is full (ms).
   *                        Pass @c UINT32_MAX to wait indefinitely.
   * @return    @ref ReturnCode::AnsweredRequest      Item posted.
   * @return    @ref ReturnCode::Full    Queue full and @p timeout_ms elapsed.
   * @return    @ref ReturnCode::ErrorTimeout Timeout expired before space appeared.
   * @return    @ref ReturnCode::Error   Handle is null.
   */
  [[nodiscard]]
  ReturnCode sendToBack(const T& item, uint32_t timeout_ms = UINT32_MAX) noexcept override
  {
    if (m_handle == nullptr)
    {
      return ReturnCode::ErrorNullPointer;
    }
    const TickType ticks = toTicks(timeout_ms);
    const BaseType rc = xQueueSendToBack(m_handle, &item, ticks);
    if (rc == pdTRUE)
    {
      return ReturnCode::AnsweredRequest;
    }
    if (count() >= m_capacity)
    {
      return ReturnCode::ErrorQueueFull;
    }
    return ReturnCode::ErrorTimeout;
  }

  /**
   * @brief     Post an item to the back of the queue from ISR context.
   * @param[in] item  Item to copy into the queue.
   * @return    @ref ReturnCode::AnsweredRequest   Item posted.
   * @return    @ref ReturnCode::Full Queue has no space.
   * @return    @ref ReturnCode::Error Handle is null.
   */
  [[nodiscard]]
  ReturnCode sendToBackFromIsr(const T& item) noexcept override
  {
    if (m_handle == nullptr)
    {
      return ReturnCode::ErrorNullPointer;
    }
    BaseType xHPTW = pdFALSE;
    const BaseType rc = xQueueSendToBackFromISR(m_handle, &item, &xHPTW);
    portYIELD_FROM_ISR(xHPTW);
    return (rc == pdTRUE) ? ReturnCode::AnsweredRequest : ReturnCode::ErrorQueueFull;
  }

  /**
   * @brief     Post an item to the front of the queue from task context.
   * @param[in] item        Item to copy.
   * @param[in] timeout_ms  Maximum wait time if full (ms).
   * @return    @ref ReturnCode::AnsweredRequest / Full / Timeout / Error.
   */
  [[nodiscard]]
  ReturnCode sendToFront(const T& item, uint32_t timeout_ms) noexcept override
  {
    if (m_handle == nullptr)
    {
      return ReturnCode::ErrorNullPointer;
    }
    const TickType ticks = toTicks(timeout_ms);
    const BaseType rc = xQueueSendToFront(m_handle, &item, ticks);
    if (rc == pdTRUE)
    {
      return ReturnCode::AnsweredRequest;
    }
    if (count() >= m_capacity)
    {
      return ReturnCode::ErrorQueueFull;
    }
    return ReturnCode::ErrorTimeout;
  }

  /**
   * @brief     Post an item to the front of the queue from ISR context.
   * @param[in] item  Item to copy.
   * @return    @ref ReturnCode::AnsweredRequest / Full / Error.
   */
  [[nodiscard]]
  ReturnCode sendToFrontFromIsr(const T& item) noexcept override
  {
    if (m_handle == nullptr)
    {
      return ReturnCode::ErrorNullPointer;
    }
    BaseType xHPTW = pdFALSE;
    const BaseType rc = xQueueSendToFrontFromISR(m_handle, &item, &xHPTW);
    portYIELD_FROM_ISR(xHPTW);
    return (rc == pdTRUE) ? ReturnCode::AnsweredRequest : ReturnCode::ErrorQueueFull;
  }

  // -------------------------------------------------------------------------
  // Receive
  // -------------------------------------------------------------------------

  /**
   * @brief      Retrieve an item from the front of the queue from task context.
   * @param[out] item        Buffer that receives the copied item.
   * @param[in]  timeout_ms  Maximum wait time if the queue is empty (ms).
   * @return     @ref ReturnCode::AnsweredRequest / Empty / Timeout / Error.
   * @warning    Buffer content is unspecified on non-Ok return.
   */
  [[nodiscard]]
  ReturnCode receive(T& item, uint32_t timeout_ms) noexcept override
  {
    if (m_handle == nullptr)
    {
      return ReturnCode::ErrorNullPointer;
    }
    const TickType ticks = toTicks(timeout_ms);
    const BaseType rc = xQueueReceive(m_handle, &item, ticks);
    if (rc == pdTRUE)
    {
      return ReturnCode::AnsweredRequest;
    }
    if (count() == 0u)
    {
      return ReturnCode::ErrorQueueEmpty;
    }
    return ReturnCode::ErrorTimeout;
  }

  /**
   * @brief      Retrieve an item from the front of the queue from ISR context.
   * @param[out] item  Buffer that receives the copied item.
   * @return     @ref ReturnCode::AnsweredRequest / Empty / Error.
   */
  [[nodiscard]]
  ReturnCode receiveFromISR(T& item) noexcept override
  {
    if (m_handle == nullptr)
    {
      return ReturnCode::ErrorNullPointer;
    }
    BaseType xHPTW = pdFALSE;
    const BaseType rc = xQueueReceiveFromISR(m_handle, &item, &xHPTW);
    portYIELD_FROM_ISR(xHPTW);
    return
        (rc == pdTRUE) ?
            ReturnCode::AnsweredRequest : ReturnCode::ErrorQueueEmpty;
  }

  // -------------------------------------------------------------------------
  // Control
  // -------------------------------------------------------------------------

  /**
   * @brief   Reset the queue, discarding all items.
   * @details Safe to call from task context only.
   */
  void reset() noexcept override
  {
    if (m_handle != nullptr)
    {
      xQueueReset(m_handle);
    }
  }

  /** @brief Alias for @ref reset(). */
  void clear() noexcept override
  {
    reset();
  }

  // -------------------------------------------------------------------------
  // State queries
  // -------------------------------------------------------------------------

  /**
   * @brief   Return the number of items currently in the queue.
   * @return  Item count, or 0 if the handle is null.
   */
  [[nodiscard]]
  uint32_t count() const noexcept override
  {
    if (m_handle == nullptr)
    {
      return 0u;
    }
    return static_cast<uint32_t>(uxQueueMessagesWaiting(m_handle));
  }

  /**
   * @brief   Return the number of items in the queue from ISR context.
   * @return  Item count, or 0 if the handle is null.
   */
  [[nodiscard]]
  uint32_t countFromISR() const noexcept override
  {
    if (m_handle == nullptr)
    {
      return 0u;
    }
    return static_cast<uint32_t>(uxQueueMessagesWaitingFromISR(m_handle));
  }

  /** @brief Return the maximum capacity of the queue. */
  [[nodiscard]]
  uint32_t capacity() const noexcept override
  {
    return m_capacity;
  }

  /** @brief Return the maximum capacity of the queue (ISR-safe). */
  [[nodiscard]]
  uint32_t capacityFromISR() const noexcept override
  {
    return m_capacity;
  }

  /**
   * @brief   Check whether the queue contains no items (task context).
   * @return  @c true if empty.
   */
  [[nodiscard]]
  bool isEmpty() const noexcept override
  {
    if (m_handle == nullptr)
    {
      return true;
    }
    return xQueueIsQueueEmptyFromISR(m_handle) == pdTRUE;
  }

  /**
   * @brief   Check whether the queue contains no items (ISR context).
   * @return  @c true if empty.
   */
  [[nodiscard]]
  bool isEmptyFromISR() const noexcept override
  {
    if (m_handle == nullptr)
    {
      return true;
    }
    return xQueueIsQueueEmptyFromISR(m_handle) == pdTRUE;
  }

  /**
   * @brief   Check whether the queue has no remaining space (task context).
   * @return  @c true if full.
   */
  [[nodiscard]]
  bool isFull() const noexcept override
  {
    if (m_handle == nullptr)
    {
      return false;
    }
    return xQueueIsQueueFullFromISR(m_handle) == pdTRUE;
  }

  /**
   * @brief   Check whether the queue has no remaining space (ISR context).
   * @return  @c true if full.
   */
  [[nodiscard]]
  bool isFullFromISR() const noexcept override
  {
    if (m_handle == nullptr)
    {
      return false;
    }
    return xQueueIsQueueFullFromISR(m_handle) == pdTRUE;
  }

protected:

  QueueHandle m_handle { nullptr }; ///< FreeRTOS queue handle; null until created.
  uint32_t m_capacity { 0u };      ///< Maximum item count set at creation.

private:

  /**
   * @brief     Convert a millisecond timeout to FreeRTOS ticks.
   * @param[in] ms  Timeout in ms; @c UINT32_MAX means @c portMAX_DELAY.
   * @return    Equivalent tick count.
   */
  static TickType toTicks(uint32_t ms) noexcept
  {
    return
        (ms == UINT32_MAX) ?
            portMAX_DELAY : static_cast<TickType>(pdMS_TO_TICKS(ms));
  }
};

// =============================================================================
// DynamicQueue<T>
// =============================================================================

/**
 * @brief   FreeRTOS queue backed by heap memory.
 * @tparam  T         Item type (must be trivially copyable).
 * @ingroup FREERTOS_QUEUE
 */
template<class T>
class DynamicQueue :
  public QueueBase<T>
{
public:

  /**
   * @brief     Construct and allocate a queue with the given capacity.
   * @param[in] capacity  Maximum number of items the queue can hold (>0).
   */
  explicit DynamicQueue(uint32_t capacity) noexcept
  {
    this->m_capacity = capacity;
    this->m_handle = xQueueCreate(static_cast<UBaseType>(capacity),
        static_cast<UBaseType>(sizeof(T)));
  }

  /**
   * @brief   Delete the FreeRTOS queue and release its heap memory.
   */
  ~DynamicQueue() noexcept override
  {
    if (this->m_handle != nullptr)
    {
      vQueueDelete(this->m_handle);
      this->m_handle = nullptr;
    }
  }
};

// =============================================================================
// StaticQueue<T, Capacity>
// =============================================================================

/**
 * @brief   FreeRTOS queue with a compile-time capacity and no heap allocation.
 * @details
 *   - Storage and the queue control block are members of this object.
 *   - Uses @c xQueueCreateStatic internally.
 * @tparam  T         Item type (must be trivially copyable).
 * @tparam  Capacity  Maximum number of items (compile-time constant, >0).
 * @ingroup FREERTOS_QUEUE
 */
template<typename T, uint32_t Capacity>
class StaticQueue :
  public QueueBase<T>
{
public:

  /** @brief Construct the static queue (no heap allocation). */
  StaticQueue() noexcept
  {
    this->m_capacity = Capacity;
    this->m_handle = xQueueCreateStatic(static_cast<UBaseType>(Capacity),
        static_cast<UBaseType>(sizeof(T)), m_storage, &m_queueBuf);
  }

private:

  StaticQueueBuf m_queueBuf { };              ///< FreeRTOS queue control block.
  alignas(T) uint8_t m_storage[Capacity * sizeof(T)] { }; ///< Item storage buffer.
};

} // namespace hel

#endif // MODULES_LIBRARIES_FREERTOS_QUEUE_HPP_
