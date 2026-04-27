/**
 ******************************************************************************
 * @file    uart.cpp
 * @author  SENAI CIMATEC (AEE Selene Team)
 * @version 1.0.0
 * @date    Apr 19, 2026
 * @brief   STM32 HAL UART driver implementation.
 ******************************************************************************
 */
#include "uart.hpp"

// ---------------------------------------------------------------------------
// File-static handle → instance registry
// ---------------------------------------------------------------------------
namespace
{
using namespace hel;
constexpr uint8_t kMaxInstances = 8U;

struct RegistryEntry
{
  UART_HandleTypeDef *handle = nullptr;
  Uart *instance = nullptr;
};

RegistryEntry s_registry[kMaxInstances] { };

void registerInstance(UART_HandleTypeDef& handle, Uart& uart) noexcept
{
  for ( auto &e : s_registry )
  {
    if (e.handle == nullptr)
    {
      e.handle = &handle;
      e.instance = &uart;
      return;
    }
  }
}

void unregisterInstance(UART_HandleTypeDef& handle) noexcept
{
  for ( auto &e : s_registry )
  {
    if (e.handle == &handle)
    {
      e.handle = nullptr;
      e.instance = nullptr;
      return;
    }
  }
}

Uart* findInstance(UART_HandleTypeDef* handle) noexcept
{
  for ( const auto &e : s_registry )
  {
    if (e.handle == handle)
    {
      return e.instance;
    }
  }
  return nullptr;
}

ReturnCode translateHalStatus(HAL_StatusTypeDef status) noexcept
{
  switch (status)
  {
    case HAL_OK :
      return ReturnCode::AnsweredRequest;
    case HAL_BUSY :
      return ReturnCode::FunctionBusy;
    case HAL_TIMEOUT :
      return ReturnCode::ErrorTimeout;
    default :
      return ReturnCode::ErrorGeneral;
  }
}

UartEvent translateHalError(UART_HandleTypeDef* huart) noexcept
{
  if ((huart->ErrorCode & HAL_UART_ERROR_PE) != 0U)
  {
    return UartEvent::ErrorParity;
  }
  if ((huart->ErrorCode & HAL_UART_ERROR_NE) != 0U)
  {
    return UartEvent::ErrorNoise;
  }
  if ((huart->ErrorCode & HAL_UART_ERROR_FE) != 0U)
  {
    return UartEvent::ErrorFraming;
  }
  if ((huart->ErrorCode & HAL_UART_ERROR_ORE) != 0U)
  {
    return UartEvent::ErrorOverrun;
  }

  __HAL_UART_CLEAR_PEFLAG(huart);
  __HAL_UART_CLEAR_NEFLAG(huart);
  __HAL_UART_CLEAR_FEFLAG(huart);
  __HAL_UART_CLEAR_OREFLAG(huart);

  return UartEvent::ErrorGeneral;
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// Uart implementation
// ---------------------------------------------------------------------------
namespace hel
{

Uart::Uart(UART_handle& handle) noexcept
    :
    m_handle(handle)
{
  registerInstance(handle, *this);
}

Uart::~Uart() noexcept
{
  unregisterInstance(m_handle);
}

// ---- Blocking transfers ---------------------------------------------------

ReturnCode Uart::write(ByteArray& data, uint32_t timeout_ms) noexcept
{
  const auto status = HAL_UART_Transmit(&m_handle, data.data(),
      static_cast<uint16_t>(data.size()), timeout_ms);
  return translateHalStatus(status);
}

ReturnCode Uart::read(ByteArray& data, uint32_t timeout_ms) noexcept
{
  const auto status = HAL_UART_Receive(&m_handle, data.data(),
      static_cast<uint16_t>(data.capacity()), timeout_ms);
  return translateHalStatus(status);
}

// ---- Interrupt transfers --------------------------------------------------

ReturnCode Uart::writeInterrupt(ByteArray& data, UartMode mode) noexcept
{
  const auto status = HAL_UART_Transmit_IT(&m_handle, data.data(),
      static_cast<uint16_t>(data.size()));
  return translateHalStatus(status);
}

ReturnCode Uart::readInterrupt(ByteArray& data, UartMode mode) noexcept
{
  HAL_StatusTypeDef status;

  if (mode == UartMode::ToIdle)
  {
    status = HAL_UARTEx_ReceiveToIdle_IT(&m_handle, data.data(),
        static_cast<uint16_t>(data.capacity()));
  }
  else
  {
    status = HAL_UART_Receive_IT(&m_handle, data.data(),
        static_cast<uint16_t>(data.capacity()));
  }

  return translateHalStatus(status);
}

// ---- DMA transfers --------------------------------------------------------

ReturnCode Uart::writeDMA(ByteArray& data, UartMode /*mode*/) noexcept
{
  const auto status = HAL_UART_Transmit_DMA(&m_handle, data.data(),
      static_cast<uint16_t>(data.size()));
  return translateHalStatus(status);
}

ReturnCode Uart::readDMA(ByteArray& data, UartMode mode) noexcept
{
  HAL_StatusTypeDef status;

  if (mode == UartMode::ToIdle)
  {
    status = HAL_UARTEx_ReceiveToIdle_DMA(&m_handle, data.data(),
        static_cast<uint16_t>(data.capacity()));
  }
  else
  {
    status = HAL_UART_Receive_DMA(&m_handle, data.data(),
        static_cast<uint16_t>(data.capacity()));
  }

  return translateHalStatus(status);
}

// ---- Abort ----------------------------------------------------------------

ReturnCode Uart::abortWrite() noexcept
{
  const auto status = HAL_UART_AbortTransmit_IT(&m_handle);
  return translateHalStatus(status);
}

ReturnCode Uart::abortRead() noexcept
{
  const auto status = HAL_UART_AbortReceive_IT(&m_handle);
  return translateHalStatus(status);
}

// ---- Callback management --------------------------------------------------

void Uart::setCallback(UartCallback callback) noexcept
{
  m_callback = callback;
}

// ---- Internal event handler -----------------------------------------------

void Uart::handleEvent(UartEvent event, uint16_t count)
{
  m_callback(event, count);
}

} // namespace hel

// ---------------------------------------------------------------------------
// HAL weak callback overrides (C linkage)
// ---------------------------------------------------------------------------
extern "C"
{
  using namespace hel;

  void HAL_UART_TxHalfCpltCallback(UART_HandleTypeDef* huart)
  {
    Uart *inst = findInstance(huart);
    if (inst != nullptr)
    {
      inst->handleEvent(UartEvent::TxHalfComplete, huart->TxXferSize / 2U);
    }
  }

  void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart)
  {
    Uart *inst = findInstance(huart);
    if (inst != nullptr)
    {
      inst->handleEvent(UartEvent::TxComplete, huart->TxXferSize);
    }
  }

  void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef* huart)
  {
    Uart *inst = findInstance(huart);
    if (inst != nullptr)
    {
      inst->handleEvent(UartEvent::RxHalfComplete, huart->RxXferSize / 2U);
    }
  }

  void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
  {
    Uart *inst = findInstance(huart);
    if (inst != nullptr)
    {
      inst->handleEvent(UartEvent::RxComplete, huart->RxXferSize);
    }
  }

// Fired for ToIdle IT/DMA; also fires for half-complete events in DMA+ToIdle.
  void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size)
  {
    Uart *inst = findInstance(huart);
    if (inst == nullptr)
    {
      return;
    }

    UartEvent event;
    switch (huart->RxEventType)
    {
      case HAL_UART_RXEVENT_HT :
        event = UartEvent::RxHalfComplete;
        break;
      case HAL_UART_RXEVENT_IDLE : // fall through
      case HAL_UART_RXEVENT_TC :
        event = UartEvent::RxComplete;
        break;
      default :
        event = UartEvent::RxComplete;
        break;
    }

    inst->handleEvent(event, Size);
  }

  void HAL_UART_AbortCpltCallback(UART_HandleTypeDef* huart)
  {
    Uart *inst = findInstance(huart);
    if (inst != nullptr)
    {
      inst->handleEvent(UartEvent::AbortComplete, 0U);
    }
  }

  void HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef* huart)
  {
    Uart *inst = findInstance(huart);
    if (inst != nullptr)
    {
      inst->handleEvent(UartEvent::AbortComplete, 0U);
    }
  }

  void HAL_UART_AbortReceiveCpltCallback(UART_HandleTypeDef* huart)
  {
    Uart *inst = findInstance(huart);
    if (inst != nullptr)
    {
      inst->handleEvent(UartEvent::AbortComplete, 0U);
    }
  }

  void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
  {
    Uart *inst = findInstance(huart);
    if (inst != nullptr)
    {
      inst->handleEvent(translateHalError(huart), 0U);
    }
  }

} // extern "C"
