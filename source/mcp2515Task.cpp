#include "mcp2515Task.h"
#include "mcp2515reg.h"
#include "gpio.h"

Mcp2515Task::Mcp2515Task(SPI* spi, uint32_t intPin, Mcp2515::Oscillator oscillator,
                         Mcp2515::Bitrate bitrate, TaskInterface::Priority priority,
                         configSTACK_DEPTH_TYPE stackDepth) :
    Mcp2515(spi, intPin, oscillator, bitrate),
    TaskInterface("canService", stackDepth, priority) {
}

void Mcp2515Task::initialInTask(RxBufferType rxBufferType) {
    _intSemaphore = xSemaphoreCreateBinary();
    _txSemaphore = xSemaphoreCreateBinary();

    resetMcp2515();
    vTaskDelay(pdMS_TO_TICKS(100));
    setMcp2515Bitrate();
    clearInterrupts(0);

    rxMode = rxBufferType;
    if (rxMode == RxBufferType::Rollover) {
        _rxSemaphore = xSemaphoreCreateCounting(2, 0);
        writeByte(RXB0CTRL, RXM_RCV_ALL | BUKT_ROLLOVER);
    } else {
        _rx0Semaphore = xSemaphoreCreateBinary();
        _rx1Semaphore = xSemaphoreCreateBinary();
        writeByte(RXB0CTRL, RXM_VALID_ALL);
        writeByte(RXB1CTRL, RXM_VALID_ALL);
    }

    gpio_init(_intPin);
    gpio_set_pulls(_intPin, true, false);
    gpioRegisterIrqCallback(_intPin, GPIO_IRQ_EDGE_FALL,
        [this](uint gpio, uint32_t events){ irqHandler(gpio, events); });

    enableInterrupts(TX0IE | RX0IE | RX1IE | MERRE | WAKIE | ERRIE);
    writeByte(CANCTRL, REQOP_NORMAL | CLKOUT_ENABLED);
}

void Mcp2515Task::irqHandler(uint gpio, uint32_t events) {
    BaseType_t woken = pdFALSE;
    xSemaphoreGiveFromISR(_intSemaphore, &woken);
    portYIELD_FROM_ISR(woken);
}

// canService: runs as its own FreeRTOS task (via TaskInterface::run).
// Reads CANINTF on each interrupt, clears flags, and gives the appropriate
// semaphore so the user task's InTask calls can unblock.
void Mcp2515Task::run() {
    while (_intSemaphore == nullptr || _txSemaphore == nullptr ||
        ((_rx0Semaphore == nullptr || _rx1Semaphore == nullptr) && _rxSemaphore == nullptr)) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    while (true) {
        while (!gpio_get(_intPin)) {
            uint8_t intFlag = readByte(CANINTF);

            if (intFlag & (MERRF | WAKIF | ERRIF)) {
                uint8_t eflg = readByte(EFLG);
                bitModify(CANINTF, MERRF | WAKIF | ERRIF, 0);
                if (eflg & 0x20) {  // TXBO: bus-off
                    writeByte(CANCTRL, REQOP_CONFIG);
                    vTaskDelay(pdMS_TO_TICKS(1));
                    writeByte(CANCTRL, REQOP_NORMAL | CLKOUT_ENABLED);
                }
                _pendingTxBuf &= ~0x01;
                xSemaphoreGive(_txSemaphore);
            } else {
                if (intFlag & RX0IF) { bitModify(CANINTF, RX0IF, 0); _pendingRxBuf |= 0x01; }
                if (intFlag & RX1IF) { bitModify(CANINTF, RX1IF, 0); _pendingRxBuf |= 0x02; }
                if (intFlag & TX0IF) { bitModify(CANINTF, TX0IF, 0); _pendingTxBuf &= ~0x01; }

                if (intFlag & RX0IF) {
                    if (rxMode == RxBufferType::Rollover) xSemaphoreGive(_rxSemaphore);
                    else                                  xSemaphoreGive(_rx0Semaphore);
                }
                if (intFlag & RX1IF) {
                    if (rxMode == RxBufferType::Rollover) xSemaphoreGive(_rxSemaphore);
                    else                                  xSemaphoreGive(_rx1Semaphore);
                }
                if (intFlag & TX0IF) { xSemaphoreGive(_txSemaphore); }
            }
        }

        xSemaphoreTake(_intSemaphore, portMAX_DELAY);
    }
}

void Mcp2515Task::txBuffer0SendInTask(uint32_t canId, uint8_t* buf, uint8_t len) {
    if (_pendingTxBuf & 0x01) {
        xSemaphoreTake(_txSemaphore, portMAX_DELAY);
    }
    _pendingTxBuf |= 0x01;
    txBuffer0Send(canId, buf, len);
}

void Mcp2515Task::rxBuffer0ReadInTask(uint32_t* canId, uint8_t* buf, uint8_t* len) {
    if ((_pendingRxBuf & 0x01) == 0) {
        xSemaphoreTake(_rx0Semaphore, portMAX_DELAY);
    }
    _pendingRxBuf &= ~0x01;
    rxBuffer0Read(canId, buf, len);
}

void Mcp2515Task::rxBuffer1ReadInTask(uint32_t* canId, uint8_t* buf, uint8_t* len) {
    if ((_pendingRxBuf & 0x02) == 0) {
        xSemaphoreTake(_rx1Semaphore, portMAX_DELAY);
    }
    _pendingRxBuf &= ~0x02;
    rxBuffer1Read(canId, buf, len);
}

void Mcp2515Task::rxReadInTask(uint32_t* canId, uint8_t* buf, uint8_t* len) {
    xSemaphoreTake(_rxSemaphore, portMAX_DELAY);
    if (_pendingRxBuf & 0x01) {
        _pendingRxBuf &= ~0x01;
        rxBuffer0Read(canId, buf, len);
    } else {
        _pendingRxBuf &= ~0x02;
        rxBuffer1Read(canId, buf, len);
    }
}
