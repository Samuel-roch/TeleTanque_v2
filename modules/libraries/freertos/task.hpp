/**
 ******************************************************************************
 * @file    task.hpp
 * @author  Samuel Almeida Rocha
 * @version 1.0.0
 * @date    2026-04-18
 * @ingroup FREERTOS_TASK
 * @brief   FreeRTOS implementation of the iTask interface.
 *
 * @details
 *   - @ref TaskBase provides lifecycle management (suspend/resume/destroy/notify).
 *   - @ref DynamicTask creates its FreeRTOS task via heap (@c xTaskCreate).
 *   - @ref StaticTask<N> creates its FreeRTOS task with a compile-time stack
 *     (@c xTaskCreateStatic); no heap allocation after construction.
 *   - Derive from @ref DynamicTask or @ref StaticTask and implement @ref run().
 *
 * @note
 *   @ref run() is called from the newly created task context. If the RTOS
 *   scheduler is already running when the constructor executes, ensure that
 *   the derived class is fully constructed before @ref run() is scheduled.
 *   Create tasks before calling @ref kernel::start() when in doubt.
 */

#ifndef MODULES_LIBRARIES_FREERTOS_TASK_HPP_
#define MODULES_LIBRARIES_FREERTOS_TASK_HPP_

#include <hel_ikernel>

namespace hel
{

/**
 * @brief   Abstract FreeRTOS task base class.
 * @details
 *   - Implements all @ref iTask lifecycle and notification methods.
 *   - Derived classes (@ref DynamicTask, @ref StaticTask) create the
 *     underlying FreeRTOS task in their constructors.
 *   - @ref run() must be implemented by the application-level class and
 *     must not return.
 * @ingroup FREERTOS_TASK
 * @note    Copy and move are deleted — tasks own OS resources.
 */
class TaskBase : public iTask
{
public:

    TaskBase() noexcept = default;

    /**
     * @brief   Destroy the task and release its FreeRTOS resources.
     * @details If the FreeRTOS task handle is still valid, @c vTaskDelete is
     *          called before the object is destroyed.
     */
    ~TaskBase() noexcept override
    {
        if (m_handle != nullptr)
        {
            vTaskDelete(m_handle);
            m_handle = nullptr;
        }
    }

    // -------------------------------------------------------------------------
    // Lifecycle  (iTask)
    // -------------------------------------------------------------------------

    /**
     * @brief     Create and start the FreeRTOS task (lazy/deferred creation).
     * @details   Use this method when the task was not created in the
     *            constructor (i.e., when *not* using @ref DynamicTask or
     *            @ref StaticTask). Must not be called if the handle is
     *            already set.
     * @param[in] name        Null-terminated task name (FreeRTOS debug only).
     * @param[in] stackWords  Stack depth in 32-bit words (>0).
     * @param[in] priority    Scheduling priority level.
     * @return    @ref TaskStatus::Ok           Task created and queued.
     * @return    @ref TaskStatus::AlreadyRunning Handle already set.
     * @return    @ref TaskStatus::InvalidParameter @p name is null or @p stackWords is 0.
     * @return    @ref TaskStatus::OutOfMemory   Insufficient heap.
     */
    [[nodiscard]]
    TaskStatus start(const char*  name,
      StackDepthType     stackWords,
                     TaskPriority priority) noexcept override
    {
        if ((name == nullptr) || (stackWords == 0u))
        {
            return TaskStatus::InvalidParameter;
        }
        if (m_handle != nullptr)
        {
            return TaskStatus::AlreadyRunning;
        }
        m_name = name;
        const BaseType rc = xTaskCreate(
            taskEntryPoint,
            name,
            stackWords,
            this,
            UBaseType(priority),
            &m_handle
        );
        if (rc != pdPASS)
        {
            m_handle = nullptr;
            return TaskStatus::OutOfMemory;
        }
        return TaskStatus::Ok;
    }

    /**
     * @brief   Query whether the task has been created.
     * @return  @ref TaskStatus::Ok         Task handle is valid.
     * @return  @ref TaskStatus::NotRunning Task has not been created.
     */
    [[nodiscard]]
    TaskStatus status() noexcept override
    {
        return (m_handle != nullptr) ? TaskStatus::Ok : TaskStatus::NotRunning;
    }

    /**
     * @brief   Suspend the task, preventing it from being scheduled.
     * @return  @ref TaskStatus::Ok         Task suspended.
     * @return  @ref TaskStatus::NotRunning Task not created.
     */
    [[nodiscard]]
    TaskStatus suspend() noexcept override
    {
        if (m_handle == nullptr)
        {
            return TaskStatus::NotRunning;
        }
        vTaskSuspend(m_handle);
        m_suspended = true;
        return TaskStatus::Ok;
    }

    /**
     * @brief   Resume a suspended task.
     * @return  @ref TaskStatus::Ok         Task resumed.
     * @return  @ref TaskStatus::NotRunning Task not created.
     */
    [[nodiscard]]
    TaskStatus resume() noexcept override
    {
        if (m_handle == nullptr)
        {
            return TaskStatus::NotRunning;
        }
        vTaskResume(m_handle);
        m_suspended = false;
        return TaskStatus::Ok;
    }

    /**
     * @brief   Delete the task and invalidate the handle.
     * @details After this call the object may be re-used via @ref start().
     * @return  @ref TaskStatus::Ok         Task deleted.
     * @return  @ref TaskStatus::NotRunning Task was not created.
     */
    [[nodiscard]]
    TaskStatus destroy() noexcept override
    {
        if (m_handle == nullptr)
        {
            return TaskStatus::NotRunning;
        }
        vTaskDelete(m_handle);
        m_handle    = nullptr;
        m_suspended = false;
        return TaskStatus::Ok;
    }

    // -------------------------------------------------------------------------
    // State queries  (iTask)
    // -------------------------------------------------------------------------

    /**
     * @brief   Check whether the task is active (created and not suspended).
     * @return  @c true if the task handle is valid and not suspended.
     */
    [[nodiscard]]
    bool isRunning() const noexcept override
    {
        return (m_handle != nullptr) && !m_suspended;
    }

    /**
     * @brief   Return the task name assigned at creation.
     * @return  Null-terminated name string, or @c nullptr if not created.
     */
    [[nodiscard]]
    const char* getName() const noexcept override
    {
        return m_name;
    }

    // -------------------------------------------------------------------------
    // Notifications  (iTask)
    // -------------------------------------------------------------------------

    /**
     * @brief   Send a notification to this task from task context.
     * @details Equivalent to a lightweight binary semaphore give.
     *          No-op if the task handle is null.
     */
    void notify() noexcept override
    {
        if (m_handle != nullptr)
        {
            xTaskNotifyGive(m_handle);
        }
    }

    /**
     * @brief   Send a notification to this task from an ISR.
     * @return  @c true if a higher-priority task was woken and a context
     *          switch was requested.
     */
    [[nodiscard]]
    bool notifyFromISR() noexcept override
    {
        if (m_handle == nullptr)
        {
            return false;
        }
        BaseType xHigherPriorityTaskWoken = pdFALSE;
        vTaskNotifyGiveFromISR(m_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        return xHigherPriorityTaskWoken == pdTRUE;
    }

    /**
     * @brief     Block the calling task waiting for a notification.
     * @param[in] timeoutMs  Maximum wait time in milliseconds.
     *                       Pass @c UINT32_MAX to wait indefinitely.
     * @return    @c true if a notification was received before the timeout.
     * @warning   Must be called from the task's own @ref run() context only.
     */
    [[nodiscard]]
    bool notifyTake(TickType timeoutMs = portMAX_DELAY) noexcept override
    {
        return ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeoutMs)) > 0u;
    }

protected:

    /** @brief Task entry point — implement the task body; must not return. */
    virtual void run() noexcept = 0;

    // -------------------------------------------------------------------------
    // Protected helpers (accessible to DynamicTask / StaticTask)
    // -------------------------------------------------------------------------

    /**
     * @brief   FreeRTOS task entry trampoline.
     * @details Passed as @c pvTaskCode to @c xTaskCreate / @c xTaskCreateStatic.
     *          Casts @p param to @ref TaskBase* and calls @ref run().
     * @param[in] param  Pointer to the @ref TaskBase instance (@c this).
     */
    static void taskEntryPoint(void* param) noexcept
    {
        static_cast<TaskBase*>(param)->run();
        vTaskDelete(nullptr); // run() must not return; clean up if it does.
    }

    TaskHandle  m_handle    { nullptr }; ///< FreeRTOS task handle; null if not created.
    const char* m_name      { nullptr }; ///< Task name pointer (not owned).
    bool        m_suspended { false };   ///< Shadow of suspend state.
};

// =============================================================================
// DynamicTask
// =============================================================================

/**
 * @brief   FreeRTOS task created on the heap at construction time.
 * @details
 *   - Calls @c xTaskCreate in the constructor with the supplied parameters.
 *   - Derive and implement @ref run().
 * @ingroup FREERTOS_TASK
 * @warning The derived class must be fully constructed before the scheduler
 *          runs @ref run(). Create tasks before @ref kernel::start() to be safe.
 */
class DynamicTask : public TaskBase
{
public:

    /**
     * @brief     Construct and immediately schedule a heap-based FreeRTOS task.
     * @param[in] name        Null-terminated task name.
     * @param[in] priority    Scheduling priority.
     * @param[in] stackDepth  Stack size in words.
     */
    explicit DynamicTask(
        const char*     name       = "",
        TaskPriority    priority   = TaskPriority::Normal,
        StackDepthType  stackDepth = static_cast<StackDepthType>(configMINIMAL_STACK_SIZE)
    ) noexcept
    {
        m_name = name;
        const BaseType rc = xTaskCreate(
            taskEntryPoint,
            name,
            stackDepth,
            this,
            UBaseType(priority),
            &m_handle
        );
        m_created = (rc == pdPASS);
    }

    /**
     * @brief   Check whether the FreeRTOS task was created successfully.
     * @return  @c true if @c xTaskCreate returned @c pdPASS.
     */
    [[nodiscard]]
    bool isCreated() const noexcept { return m_created; }

private:

    bool m_created { false }; ///< Creation success flag.
};

// =============================================================================
// StaticTask<N>
// =============================================================================

/**
 * @brief   FreeRTOS task with a compile-time fixed stack (no heap).
 * @details
 *   - Calls @c xTaskCreateStatic in the constructor.
 *   - Stack storage and the TCB buffer are members of this object.
 *   - Derive and implement @ref run().
 * @tparam  N  Stack depth in words (default: @c configMINIMAL_STACK_SIZE).
 * @ingroup FREERTOS_TASK
 * @warning Same construction-vs-scheduling caveat as @ref DynamicTask applies.
 */
template<UBaseType N>
class StaticTask : public TaskBase
{
public:

    /**
     * @brief     Construct and immediately schedule a statically-allocated task.
     * @param[in] name      Null-terminated task name.
     * @param[in] priority  Scheduling priority.
     */
    explicit StaticTask(
        const char*  name     = "",
        TaskPriority priority = TaskPriority::Normal
    ) noexcept
    {
        m_name   = name;
        m_handle = xTaskCreateStatic(
            taskEntryPoint,
            name,
            N,
            this,
            UBaseType(priority),
            m_stack,
            &m_taskBuf
        );
    }

private:

    StaticTaskBuf m_taskBuf {};           ///< FreeRTOS TCB buffer (static allocation).
    StackType     m_stack[N] {};          ///< Task stack (static allocation).
};

} // namespace hel

#endif // MODULES_LIBRARIES_FREERTOS_TASK_HPP_
