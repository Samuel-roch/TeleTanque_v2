/**
 ******************************************************************************
 * @file    gpio.hpp
 * @author  Samuel Almeida Rocha
 * @version 2.0.0
 * @date    2026-04-24
 * @ingroup HELIOS_DRV_GPIO
 * @brief   STM32 HAL implementation of @ref iGpio.
 *
 * @details
 *   - Maps @ref iGpio to the STM32 HAL GPIO and EXTI peripheral.
 *   - Accepts an @ref IoPortPin value that encodes both the port (A–K) and the
 *     pin number (0–15) in a single 16-bit value.
 *   - Maintains a class-level static IRQ slot table so that the HAL EXTI weak
 *     callbacks can be dispatched to the correct instance without any user glue.
 *   - Only one @ref Gpio instance may occupy each EXTI line (pin 0–15). A second
 *     instance on the same line replaces the previous registration.
 *
 * @note
 *   - GPIO direction, pull-up/pull-down and alternate function are configured
 *     by the BSP layer (CubeMX / HAL_GPIO_Init) before instantiating this class.
 *   - NVIC interrupt priority and enable must also be set by the BSP layer.
 *   - @ref enableInterrupt registers the software callback; it does not alter
 *     any hardware register.
 *   - Thread-safety: @ref write, @ref toggle, @ref read may be called from any
 *     context. @ref enableInterrupt must not be called concurrently with an
 *     in-progress ISR.
 */

#ifndef MODULES_DRIVERS_GPIO_GPIO_HPP_
#define MODULES_DRIVERS_GPIO_GPIO_HPP_

#include <hel_igpio>
#include <hel_target>
#include <cstdint>
#include <array>

namespace hel
{

/**
 * @class  Gpio
 * @brief  STM32 HAL implementation of @ref iGpio.
 * @ingroup HELIOS_DRV_GPIO
 *
 * ### Example — output
 * @code
 *   hel::Gpio led(IoPortPin::PB0);
 *   led.write(true);
 *   led.toggle();
 * @endcode
 *
 * ### Example — input with interrupt
 * @code
 *   hel::Gpio btn(IoPortPin::PC13);
 *   hel::GpioCallback cb;
 *   cb.set(&myObj, &MyClass::onPress);
 *   btn.enableInterrupt(hel::GpioIrqMode::Falling, cb);
 * @endcode
 */
class Gpio : public iGpio
{
public:
  // -------------------------------------------------------------------------
  // Construction / destruction
  // -------------------------------------------------------------------------

  // Construct a Gpio bound to the given IoPortPin.
  explicit Gpio(IoPortPin pin);

  // Destructor — removes this instance from the EXTI dispatch table.
  ~Gpio() noexcept override;

  /** @brief Deleted — GPIO instances are non-copyable singletons. */
  Gpio(const Gpio&) = delete;
  /** @brief Deleted — GPIO instances are non-copyable singletons. */
  Gpio& operator=(const Gpio&) = delete;
  /** @brief Deleted — GPIO instances are non-movable singletons. */
  Gpio(Gpio&&) = delete;
  /** @brief Deleted — GPIO instances are non-movable singletons. */
  Gpio& operator=(Gpio&&) = delete;

  // -------------------------------------------------------------------------
  // iGpio — digital I/O
  // -------------------------------------------------------------------------

  // Read the current logical level of the pin.
  ReturnCode read(bool& level) const noexcept override;

  // Drive the pin to the requested logical level.
  ReturnCode write(bool level) noexcept override;

  // Toggle the current output level of the pin.
  ReturnCode toggle() noexcept override;

  // -------------------------------------------------------------------------
  // iGpio — interrupt
  // -------------------------------------------------------------------------

  // Arm the EXTI interrupt with the given trigger mode and callback.
  void enableInterrupt(GpioIrqMode mode, GpioCallback callback) noexcept override;

  // -------------------------------------------------------------------------
  // Accessors
  // -------------------------------------------------------------------------

  // Return the currently configured interrupt trigger mode.
  [[nodiscard]] GpioIrqMode interruptMode() const noexcept;

  // -------------------------------------------------------------------------
  // ISR dispatch
  // -------------------------------------------------------------------------

  // Look up the Gpio instance registered for a given EXTI pin mask.
  static Gpio* find_by_pinmask(uint16_t pin_mask) noexcept;

  // -------------------------------------------------------------------------
  // iGpio — internal ISR handler
  // -------------------------------------------------------------------------

  // Invoke the registered callback with the triggering edge.
  void handleIrq(GpioIrqMode cause) override;

private:
  // -------------------------------------------------------------------------
  // IRQ slot table
  // -------------------------------------------------------------------------

  /**
   * @brief Associates an EXTI pin mask with its owning @ref Gpio instance.
   */
  struct IrqSlot
  {
    uint16_t pin_mask = 0U;      /*!< STM32 GPIO pin bit-mask, or 0 when free. */
    Gpio*    instance = nullptr; /*!< Owning Gpio instance, or nullptr when free. */
  };

  /** @brief Maximum number of concurrent EXTI lines (STM32 supports 0–15). */
  static constexpr std::size_t kMaxIrqSlots = 16U;

  /**
   * @brief Per-EXTI-line dispatch table shared by all @ref Gpio instances.
   * @details Zero-initialised; entries are claimed in @ref enableInterrupt and
   *          released in the destructor.
   */
  static std::array<IrqSlot, kMaxIrqSlots> s_irq_slots;

  // -------------------------------------------------------------------------
  // Data members
  // -------------------------------------------------------------------------

  GPIO_TypeDef* m_port;           /*!< Resolved STM32 port pointer; nullptr if invalid. */
  uint16_t      m_pin_arm;        /*!< STM32 GPIO pin bit-mask (1 << pin_number).        */
  GpioIrqMode   m_interrupt_mode; /*!< Active interrupt trigger mode.                   */
  GpioCallback  m_callback;       /*!< User-registered interrupt callback.               */
};

} // namespace hel

#endif // MODULES_DRIVERS_GPIO_GPIO_HPP_
