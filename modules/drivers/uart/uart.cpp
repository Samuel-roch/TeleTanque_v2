/**
 ******************************************************************************
 * @file    uart.cpp
 * @author  SENAI CIMATEC (AEE Selene Team)
 * @version 1.0.0
 * @date    Apr 19, 2026
 * @brief   STM32 HAL UART driver implementation.
 ******************************************************************************
 */
#include <uart/uart.hpp>

// ---------------------------------------------------------------------------
// File-static handle → instance registry
// ---------------------------------------------------------------------------
namespace
{
using namespace hel;
constexpr uint8_t kMaxInstances = 8U;

struct RegistryEntry
{
    UART_HandleTypeDef* handle   = nullptr;
    Uart*          instance = nullptr;
};

RegistryEntry s_registry[kMaxInstances]{};

void registerInstance(UART_HandleTypeDef& handle, Uart& uart) noexcept
{
    for (auto& e : s_registry)
    {
        if (e.handle == nullptr)
        {
            e.handle   = &handle;
            e.instance = &uart;
            return;
        }
    }
}

void unregisterInstance(UART_HandleTypeDef& handle) noexcept
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

Uart* findInstance(UART_HandleTypeDef* handle) noexcept
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

UartStatus translateHalStatus(HAL_StatusTypeDef status) noexcept
{
    switch (status)
    {
        case HAL_OK:      return UartStatus::Ok;
        case HAL_BUSY:    return UartStatus::Busy;
        case HAL_TIMEOUT: return UartStatus::Timeout;
        default:          return UartStatus::ErrorGeneral;
    }
}

UartStatus translateHalError(uint32_t error_code) noexcept
{
    if ((error_code & HAL_UART_ERROR_PE)  != 0U) { return UartStatus::ErrorParity;  }
    if ((error_code & HAL_UART_ERROR_NE)  != 0U) { return UartStatus::ErrorNoise;   }
    if ((error_code & HAL_UART_ERROR_FE)  != 0U) { return UartStatus::ErrorFraming; }
    if ((error_code & HAL_UART_ERROR_ORE) != 0U) { return UartStatus::ErrorOverrun; }
    return UartStatus::ErrorGeneral;
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

UartStatus Uart::write(ConstByteArray& data, uint32_t timeout_ms) noexcept
{
    const auto status = HAL_UART_Transmit(
        &m_handle,
        data.data(),
        static_cast<uint16_t>(data.size()),
        timeout_ms);
    return translateHalStatus(status);
}

UartStatus Uart::read(ByteArray& data, uint32_t timeout_ms) noexcept
{
    const auto status = HAL_UART_Receive(
        &m_handle,
        data.data(),
        static_cast<uint16_t>(data.size()),
        timeout_ms);
    return translateHalStatus(status);
}

// ---- Interrupt transfers --------------------------------------------------

UartStatus Uart::writeInterrupt(ConstByteArray& data, SerialMode /*mode*/) noexcept
{
    const auto status = HAL_UART_Transmit_IT(
        &m_handle,
        data.data(),
        static_cast<uint16_t>(data.size()));
    return translateHalStatus(status);
}

UartStatus Uart::readInterrupt(ByteArray& data, SerialMode mode) noexcept
{
    HAL_StatusTypeDef status;

    if (mode == SerialMode::ToIdle)
    {
        status = HAL_UARTEx_ReceiveToIdle_IT(
            &m_handle,
            data.data(),
            static_cast<uint16_t>(data.size()));
    }
    else
    {
        status = HAL_UART_Receive_IT(
            &m_handle,
            data.data(),
            static_cast<uint16_t>(data.size()));
    }

    return translateHalStatus(status);
}

// ---- DMA transfers --------------------------------------------------------

UartStatus Uart::writeDMA(ConstByteArray& data, SerialMode /*mode*/) noexcept
{
    const auto status = HAL_UART_Transmit_DMA(
        &m_handle,
        data.data(),
        static_cast<uint16_t>(data.size()));
    return translateHalStatus(status);
}

UartStatus Uart::readDMA(ByteArray& data, SerialMode mode) noexcept
{
    HAL_StatusTypeDef status;

    if (mode == SerialMode::ToIdle)
    {
        status = HAL_UARTEx_ReceiveToIdle_DMA(
            &m_handle,
            data.data(),
            static_cast<uint16_t>(data.size()));
    }
    else
    {
        status = HAL_UART_Receive_DMA(
            &m_handle,
            data.data(),
            static_cast<uint16_t>(data.size()));
    }

    return translateHalStatus(status);
}

// ---- Abort ----------------------------------------------------------------

UartStatus Uart::abortWrite() noexcept
{
    const auto status = HAL_UART_AbortTransmit_IT(&m_handle);
    return translateHalStatus(status);
}

UartStatus Uart::abortRead() noexcept
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

void Uart::handleEvent(UartStatus event, uint16_t count)
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
    Uart* inst = findInstance(huart);
    if (inst != nullptr)
    {
        inst->handleEvent(UartStatus::TxHalfComplete, huart->TxXferSize / 2U);
    }
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef* huart)
{
    Uart* inst = findInstance(huart);
    if (inst != nullptr)
    {
        inst->handleEvent(UartStatus::TxComplete, huart->TxXferSize);
    }
}

void HAL_UART_RxHalfCpltCallback(UART_HandleTypeDef* huart)
{
    Uart* inst = findInstance(huart);
    if (inst != nullptr)
    {
        inst->handleEvent(UartStatus::RxHalfComplete, huart->RxXferSize / 2U);
    }
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef* huart)
{
    Uart* inst = findInstance(huart);
    if (inst != nullptr)
    {
        inst->handleEvent(UartStatus::RxComplete, huart->RxXferSize);
    }
}

// Fired for ToIdle IT/DMA; also fires for half-complete events in DMA+ToIdle.
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size)
{
    Uart* inst = findInstance(huart);
    if (inst == nullptr) { return; }

    UartStatus event;
    switch (huart->RxEventType)
    {
        case HAL_UART_RXEVENT_HT:
            event = UartStatus::RxHalfComplete;
            break;
        case HAL_UART_RXEVENT_IDLE: // fall through
        case HAL_UART_RXEVENT_TC:
            event = UartStatus::RxComplete;
            break;
        default:
            event = UartStatus::RxComplete;
            break;
    }

    inst->handleEvent(event, Size);
}

void HAL_UART_AbortCpltCallback(UART_HandleTypeDef* huart)
{
    Uart* inst = findInstance(huart);
    if (inst != nullptr)
    {
        inst->handleEvent(UartStatus::AbortComplete, 0U);
    }
}

void HAL_UART_AbortTransmitCpltCallback(UART_HandleTypeDef* huart)
{
    Uart* inst = findInstance(huart);
    if (inst != nullptr)
    {
        inst->handleEvent(UartStatus::AbortComplete, 0U);
    }
}

void HAL_UART_AbortReceiveCpltCallback(UART_HandleTypeDef* huart)
{
    Uart* inst = findInstance(huart);
    if (inst != nullptr)
    {
        inst->handleEvent(UartStatus::AbortComplete, 0U);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{
    Uart* inst = findInstance(huart);
    if (inst != nullptr)
    {
        inst->handleEvent(translateHalError(huart->ErrorCode), 0U);
    }
}

} // extern "C"
