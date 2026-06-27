#include "uart.h"
#include "hardware/gpio.h"

UART* UART::_instances[2] = {nullptr, nullptr};

UART::UART(uart_inst_t* uart, uint32_t baudrate, uint32_t tx, uint32_t rx,
           bool enableTxDma, bool enableRxDma,
           uint8_t* rxBuffer, uint16_t rxHalfSize, uint32_t idleTimeoutMs)
    : _uart(uart), _enableTxDma(enableTxDma), _enableRxDma(enableRxDma),
      _rxBuffer(rxBuffer), _rxHalfSize(rxHalfSize), _rxActiveHalf(0),
      _rxBytesReady(0), _rxDmaActive(false), _idleTimeoutMs(idleTimeoutMs) {
    uart_init(uart, baudrate);
    gpio_set_function(tx, GPIO_FUNC_UART);
    gpio_set_function(rx, GPIO_FUNC_UART);

    if (enableTxDma) {
        dmaTxSemaphore = xSemaphoreCreateBinary();
        dmaTx = DMA::enableDma(dmaTxCallback, dmaTxSemaphore);
        dmaTxConfigure();
    }

    if (enableRxDma && _rxBuffer && _rxHalfSize > 0) {
        dmaRxSemaphore = xSemaphoreCreateBinary();
        dmaRx = DMA::enableDma(dmaRxCallback, this);
        dmaRxConfigure();

        _rxIdleTimer = xTimerCreate("uartRxIdle",
            pdMS_TO_TICKS(_idleTimeoutMs), pdFALSE, this, rxIdleTimerCallback);

        // Register IRQ handler for this UART peripheral
        uint8_t idx = uart_get_index(_uart);
        _instances[idx] = this;
        int irqNum = idx ? UART1_IRQ : UART0_IRQ;
        irq_add_shared_handler(irqNum, idx ? uart1IrqHandler : uart0IrqHandler,
            PICO_SHARED_IRQ_HANDLER_DEFAULT_ORDER_PRIORITY);
        irq_set_enabled(irqNum, true);

        // RXIM: FIFO >= 1/8 full (4 bytes). RTIM: RX timeout for sub-threshold packets.
        enableRxIrq();
    }
}

// --- TX ---

void UART::dmaTxConfigure() {
    dmaTxConfig = dma_channel_get_default_config(dmaTx);
    channel_config_set_transfer_data_size(&dmaTxConfig, DMA_SIZE_8);
    channel_config_set_dreq(&dmaTxConfig, uart_get_dreq(_uart, true));
    // defaults: read_increment=true, write_increment=false
    dma_channel_configure(dmaTx, &dmaTxConfig, &uart_get_hw(_uart)->dr, nullptr, 0, false);
}

uint16_t UART::write(const void* data, uint16_t size, uint32_t timeout_ms) {
    if (_enableTxDma) {
        dmaWrite(data, size);
        return xSemaphoreTake(dmaTxSemaphore, pdMS_TO_TICKS(timeout_ms)) == pdTRUE ? size : 0;
    }
    uart_write_blocking(_uart, reinterpret_cast<const uint8_t*>(data), size);
    return size;
}

void UART::dmaWrite(const void* data, uint16_t size) {
    dma_channel_set_read_addr(dmaTx, data, false);
    dma_channel_set_trans_count(dmaTx, size, true);
}

void UART::dmaTxCallback(void* param) {
    BaseType_t xHigherPriorityTaskWoken;
    xSemaphoreGiveFromISR(reinterpret_cast<SemaphoreHandle_t>(param), &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// --- RX ---

void UART::dmaRxConfigure() {
    dmaRxConfig = dma_channel_get_default_config(dmaRx);
    channel_config_set_transfer_data_size(&dmaRxConfig, DMA_SIZE_8);
    channel_config_set_dreq(&dmaRxConfig, uart_get_dreq(_uart, false));
    channel_config_set_read_increment(&dmaRxConfig, false);
    channel_config_set_write_increment(&dmaRxConfig, true);
    dma_channel_configure(dmaRx, &dmaRxConfig, nullptr, &uart_get_hw(_uart)->dr, 0, false);
}

void UART::startRxDma() {
    dma_channel_set_write_addr(dmaRx, _rxBuffer + (_rxActiveHalf * _rxHalfSize), false);
    dma_channel_set_trans_count(dmaRx, _rxHalfSize, true);
    _rxDmaActive = true;
}

uint16_t UART::rxBytesReceived() const {
    uint32_t writeAddr = dma_channel_hw_addr(dmaRx)->write_addr;
    uint32_t startAddr = (uint32_t)(_rxBuffer + (_rxActiveHalf * _rxHalfSize));
    return (uint16_t)(writeAddr - startAddr);
}

void UART::enableRxIrq() {
    uart_get_hw(_uart)->imsc |= UART_UARTIMSC_RXIM_BITS | UART_UARTIMSC_RTIM_BITS;
}

void UART::disableRxIrq() {
    uart_get_hw(_uart)->imsc &= ~(UART_UARTIMSC_RXIM_BITS | UART_UARTIMSC_RTIM_BITS);
}

// Called from UART IRQ: data detected in FIFO, DMA not yet running.
void UART::uartIrqCommon(UART* self) {
    uint32_t mis = uart_get_hw(self->_uart)->mis;
    if (!(mis & (UART_UARTMIS_RXMIS_BITS | UART_UARTMIS_RTMIS_BITS))) return;

    uart_get_hw(self->_uart)->icr = UART_UARTICR_RXIC_BITS | UART_UARTICR_RTIC_BITS;

    if (!self->_rxDmaActive) {
        // Disable UART IRQ — DMA takes over byte draining from here
        self->disableRxIrq();
        self->startRxDma();
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTimerResetFromISR(self->_rxIdleTimer, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

void UART::uart0IrqHandler() { uartIrqCommon(_instances[0]); }
void UART::uart1IrqHandler() { uartIrqCommon(_instances[1]); }

// DMA half complete: full rxHalfSize bytes received. Restart immediately (ping-pong).
void UART::dmaRxCallback(void* param) {
    UART* self = reinterpret_cast<UART*>(param);

    // Guard: rxIdleTimerCallback may have aborted DMA and cleared _rxDmaActive.
    // Without this, a spurious completion after abort restarts DMA and double-signals.
    UBaseType_t saved = taskENTER_CRITICAL_FROM_ISR();
    if (!self->_rxDmaActive) {
        taskEXIT_CRITICAL_FROM_ISR(saved);
        return;
    }
    self->_rxActiveHalf ^= 1;
    self->_rxBytesReady = self->_rxHalfSize;
    self->startRxDma();  // restart before yielding — minimises gap between transfers
    taskEXIT_CRITICAL_FROM_ISR(saved);

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTimerResetFromISR(self->_rxIdleTimer, &xHigherPriorityTaskWoken);
    xSemaphoreGiveFromISR(self->dmaRxSemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

// Idle timeout: no DMA half completed for idleTimeoutMs. Abort and report partial data.
void UART::rxIdleTimerCallback(TimerHandle_t timer) {
    UART* self = reinterpret_cast<UART*>(pvTimerGetTimerID(timer));

    taskENTER_CRITICAL();
    if (!self->_rxDmaActive) {
        taskEXIT_CRITICAL();
        return;
    }
    dma_channel_abort(self->dmaRx);
    uint16_t bytes = self->rxBytesReceived();
    self->_rxActiveHalf ^= 1;
    self->_rxDmaActive = false;
    self->_rxBytesReady = bytes;
    // Re-arm UART IRQ to detect next message start
    self->enableRxIrq();
    taskEXIT_CRITICAL();

    if (bytes > 0) {
        xSemaphoreGive(self->dmaRxSemaphore);
    }
}

const uint8_t* UART::read(uint16_t* size, uint32_t timeout_ms) {
    if (!_enableRxDma || !_rxBuffer) {
        *size = 0;
        return nullptr;
    }
    if (xSemaphoreTake(dmaRxSemaphore, pdMS_TO_TICKS(timeout_ms)) != pdTRUE) {
        *size = 0;
        return nullptr;
    }
    *size = _rxBytesReady;
    return _rxBuffer + ((_rxActiveHalf ^ 1) * _rxHalfSize);
}

uint16_t UART::readBlocking(void* data, uint16_t size) {
    uart_read_blocking(_uart, reinterpret_cast<uint8_t*>(data), size);
    return size;
}
