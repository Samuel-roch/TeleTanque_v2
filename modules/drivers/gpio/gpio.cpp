/**
 ******************************************************************************
 * @file    gpio.cpp
 * @author  Samuel Almeida Rocha
 * @version 2.0.0
 * @date    2026-04-24
 * @ingroup HELIOS_DRV_GPIO
 * @brief   STM32 HAL implementation of @ref Gpio.
 ******************************************************************************
 */

#include "gpio.hpp"

#if defined(HAL_GPIO_MODULE_ENABLED)

namespace hel
{

// =============================================================================
// Static member definitions
// =============================================================================

/**
 * @brief Per-EXTI-line dispatch table.
 * @details Zero-initialised at program start.  Each entry is claimed by
 *          @ref Gpio::enableInterrupt and released by @ref Gpio::~Gpio.
 */
std::array<Gpio::IrqSlot, Gpio::kMaxIrqSlots> Gpio::s_irq_slots = {};

// =============================================================================
// Port lookup table
// =============================================================================

/**
 * @brief Compile-time port pointer table indexed by port index (A=0, B=1, …).
 * @details Entries for ports not present on the target MCU are `nullptr`;
 *          @ref Gpio::Gpio guards against using such entries.
 */
static const GPIO_TypeDef* const kPortList[] =
{
#if defined(GPIOA)
  GPIOA,   // 0 — Port A
#else
  nullptr,
#endif
#if defined(GPIOB)
  GPIOB,   // 1 — Port B
#else
  nullptr,
#endif
#if defined(GPIOC)
  GPIOC,   // 2 — Port C
#else
  nullptr,
#endif
#if defined(GPIOD)
  GPIOD,   // 3 — Port D
#else
  nullptr,
#endif
#if defined(GPIOE)
  GPIOE,   // 4 — Port E
#else
  nullptr,
#endif
#if defined(GPIOF)
  GPIOF,   // 5 — Port F
#else
  nullptr,
#endif
#if defined(GPIOG)
  GPIOG,   // 6 — Port G
#else
  nullptr,
#endif
#if defined(GPIOH)
  GPIOH,   // 7 — Port H
#else
  nullptr,
#endif
#if defined(GPIOI)
  GPIOI,   // 8 — Port I
#else
  nullptr,
#endif
#if defined(GPIOJ)
  GPIOJ,   // 9 — Port J
#else
  nullptr,
#endif
#if defined(GPIOK)
  GPIOK,   // 10 — Port K
#else
  nullptr,
#endif
};

// =============================================================================
// Construction / destruction
// =============================================================================

/**
 * @brief Construct a new Gpio object with a specified port-pin combination.
 * @details Extracts the port index (bits [7:4]) and pin number (bits [3:0]) from
 *          @p pin, looks up the port pointer in @ref kPortList, and computes the
 *          STM32 pin bit-mask.  If the port index is out of range @ref m_port is
 *          left `nullptr` and all operations will return @ref ReturnCode::NotInitialized.
 *
 * @param[in] pin  @ref IoPortPin value encoding port (A–K) and pin (0–15).
 */
Gpio::Gpio(IoPortPin pin)
  :
  m_port(nullptr),
  m_pin_arm(0U),
  m_interrupt_mode(GpioIrqMode::Disabled)
{
  const uint16_t val    = static_cast<uint16_t>(pin);
  const uint8_t  port   = static_cast<uint8_t>((val & 0xF0U) >> 4U);
  const uint8_t  pinNum = static_cast<uint8_t>(val & 0x0FU);

  constexpr uint8_t kNumPorts =
      static_cast<uint8_t>(sizeof(kPortList) / sizeof(kPortList[0]));

  if (port < kNumPorts && kPortList[port] != nullptr)
  {
    // const_cast is safe: HAL functions accept non-const GPIO_TypeDef*
    m_port    = const_cast<GPIO_TypeDef*>(kPortList[port]);
    m_pin_arm = static_cast<uint16_t>(1U << pinNum);
  }
}

/**
 * @brief Destructor — removes this instance from the EXTI dispatch table.
 * @details Iterates @ref s_irq_slots and clears any entry whose @p instance
 *          pointer matches `this`, ensuring that subsequent interrupts on the
 *          same EXTI line are silently ignored.
 */
Gpio::~Gpio() noexcept
{
  for (auto& slot : s_irq_slots)
  {
    if (slot.instance == this)
    {
      slot.pin_mask = 0U;
      slot.instance = nullptr;
      break;
    }
  }
}

// =============================================================================
// iGpio — digital I/O
// =============================================================================

/**
 * @brief Read the current logical level of the pin.
 * @details Delegates to `HAL_GPIO_ReadPin`.  The result is `true` for
 *          `GPIO_PIN_SET` and `false` for `GPIO_PIN_RESET`.
 *
 * @param[out] level  Populated with the current pin state on success.
 * @return @ref ReturnCode::AnsweredRequest on success.
 * @return @ref ReturnCode::NotInitialized if @ref m_port is `nullptr`.
 */
ReturnCode Gpio::read(bool& level) const noexcept
{
  if (m_port == nullptr)
  {
    return ReturnCode::NotInitialized;
  }
  level = static_cast<bool>(HAL_GPIO_ReadPin(m_port, m_pin_arm));
  return ReturnCode::AnsweredRequest;
}

/**
 * @brief Drive the pin to the requested logical level.
 * @details Delegates to `HAL_GPIO_WritePin`, converting the boolean @p level to
 *          `GPIO_PIN_SET` or `GPIO_PIN_RESET`.
 *
 * @param[in] level  `true` drives the pin high; `false` drives it low.
 * @return @ref ReturnCode::AnsweredRequest on success.
 * @return @ref ReturnCode::NotInitialized if @ref m_port is `nullptr`.
 */
ReturnCode Gpio::write(bool level) noexcept
{
  if (m_port == nullptr)
  {
    return ReturnCode::NotInitialized;
  }
  HAL_GPIO_WritePin(m_port, m_pin_arm, static_cast<GPIO_PinState>(level));
  return ReturnCode::AnsweredRequest;
}

/**
 * @brief Toggle the current output level of the pin.
 * @details Delegates to `HAL_GPIO_TogglePin`.
 *
 * @return @ref ReturnCode::AnsweredRequest on success.
 * @return @ref ReturnCode::NotInitialized if @ref m_port is `nullptr`.
 */
ReturnCode Gpio::toggle() noexcept
{
  if (m_port == nullptr)
  {
    return ReturnCode::NotInitialized;
  }
  HAL_GPIO_TogglePin(m_port, m_pin_arm);
  return ReturnCode::AnsweredRequest;
}

// =============================================================================
// iGpio — interrupt
// =============================================================================

/**
 * @brief Arm the EXTI interrupt for this pin.
 * @details Searches @ref s_irq_slots for an existing entry matching
 *          @ref m_pin_arm and updates it; if none is found, the first free slot
 *          (where @p instance is `nullptr`) is claimed.  @p mode and @p callback
 *          are stored regardless of whether a free slot was found, so the
 *          instance can still receive events if it is re-registered later.
 *
 * @param[in] mode      Trigger edge(s) to record and forward to the callback.
 * @param[in] callback  Callable invoked from ISR context on each EXTI event.
 */
void Gpio::enableInterrupt(GpioIrqMode mode, GpioCallback callback) noexcept
{
  m_interrupt_mode = mode;
  m_callback       = callback;

  // Update existing slot for this pin, or claim the first free one
  for (auto& slot : s_irq_slots)
  {
    if (slot.instance == this || slot.instance == nullptr)
    {
      slot.pin_mask = m_pin_arm;
      slot.instance = this;
      return;
    }
  }
  // All slots occupied — callback is stored but won't be dispatched via ISR
}

// =============================================================================
// Accessors
// =============================================================================

/**
 * @brief Return the currently configured interrupt trigger mode.
 * @return The @ref GpioIrqMode set by the last @ref enableInterrupt call, or
 *         @ref GpioIrqMode::Disabled if @ref enableInterrupt has never been called.
 */
GpioIrqMode Gpio::interruptMode() const noexcept
{
  return m_interrupt_mode;
}

// =============================================================================
// ISR dispatch
// =============================================================================

/**
 * @brief Look up the @ref Gpio instance registered for a given pin mask.
 * @details Linear search through @ref s_irq_slots comparing @p pin_mask against
 *          each entry's stored mask.
 *
 * @param[in] pin_mask  STM32 GPIO pin bit-mask supplied by the HAL callback
 *                      (e.g. `GPIO_PIN_5` = `0x0020`).
 * @return Pointer to the matching @ref Gpio, or `nullptr` if none is registered.
 */
Gpio* Gpio::find_by_pinmask(uint16_t pin_mask) noexcept
{
  for (auto& slot : s_irq_slots)
  {
    if (slot.pin_mask == pin_mask)
    {
      return slot.instance;
    }
  }
  return nullptr;
}

// =============================================================================
// iGpio — internal ISR handler
// =============================================================================

/**
 * @brief Invoke the registered callback with the triggering edge.
 * @details Called exclusively from the HAL weak callbacks below, in ISR context.
 *          Forwards @p cause directly to the stored @ref GpioCallback.
 *
 * @param[in] cause  @ref GpioIrqMode value identifying the edge that fired.
 */
void Gpio::handleIrq(GpioIrqMode cause)
{
  m_callback(cause);
}

} // namespace hel

// =============================================================================
// HAL weak callback overrides (extern "C", ISR context)
// =============================================================================

// STM32H7 HAL (>= 1.10) provides separate rising / falling callbacks.
// Detected by the presence of the H7-specific clear macro.
#if defined(__HAL_GPIO_EXTI_CLEAR_FALLING_IT)

/**
 * @brief HAL weak callback — falling edge on any EXTI line (H7 HAL >= 1.10).
 * @details Dispatches to the @ref Gpio instance registered for @p GPIO_Pin.
 *          Only calls @ref Gpio::handleIrq when the instance is configured for
 *          @ref GpioIrqMode::Falling or @ref GpioIrqMode::Change.
 *
 * @param[in] GPIO_Pin  Single-bit STM32 pin mask that triggered the interrupt.
 */
extern "C" void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  hel::Gpio* drv = hel::Gpio::find_by_pinmask(GPIO_Pin);
  if (drv == nullptr) { return; }

  const hel::GpioIrqMode mode = drv->interruptMode();
  if (mode == hel::GpioIrqMode::Falling || mode == hel::GpioIrqMode::Change)
  {
    drv->handleIrq(hel::GpioIrqMode::Falling);
  }
}

/**
 * @brief HAL weak callback — rising edge on any EXTI line (H7 HAL >= 1.10).
 * @details Dispatches to the @ref Gpio instance registered for @p GPIO_Pin.
 *          Only calls @ref Gpio::handleIrq when the instance is configured for
 *          @ref GpioIrqMode::Rising or @ref GpioIrqMode::Change.
 *
 * @param[in] GPIO_Pin  Single-bit STM32 pin mask that triggered the interrupt.
 */
extern "C" void HAL_GPIO_EXTI_Rising_Callback(uint16_t GPIO_Pin)
{
  hel::Gpio* drv = hel::Gpio::find_by_pinmask(GPIO_Pin);
  if (drv == nullptr) { return; }

  const hel::GpioIrqMode mode = drv->interruptMode();
  if (mode == hel::GpioIrqMode::Rising || mode == hel::GpioIrqMode::Change)
  {
    drv->handleIrq(hel::GpioIrqMode::Rising);
  }
}

#else

/**
 * @brief HAL weak callback — any EXTI edge (classic HAL, F4 / L4 / F1 / F3 …).
 * @details Dispatches to the @ref Gpio instance registered for @p GPIO_Pin and
 *          forwards @ref Gpio::interruptMode as the cause so the callback can
 *          identify which edge(s) the pin was configured for.
 *
 * @param[in] GPIO_Pin  Single-bit STM32 pin mask that triggered the interrupt.
 */
extern "C" void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  hel::Gpio* drv = hel::Gpio::find_by_pinmask(GPIO_Pin);
  if (drv == nullptr) { return; }

  drv->handleIrq(drv->interruptMode());
}

#endif // __HAL_GPIO_EXTI_CLEAR_FALLING_IT

#endif // HAL_GPIO_MODULE_ENABLED
