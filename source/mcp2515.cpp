#include "mcp2515.h"
#include "mcp2515reg.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

static SemaphoreHandle_t txSemaphore[2] {};
static void irqCallback0(uint gpio, uint32_t events);
static void irqCallback1(uint gpio, uint32_t events);

Mcp2515::Mcp2515(SPI* spi, uint32_t intPin, Oscillator oscillator, Bitrate bitrate) :
    _spi(spi), bitrateAddr(oscillator == Oscillator::MHZ8 ?
        &bitrateToReg_8MHz[static_cast<uint8_t>(bitrate)][0] :
        &bitrateToReg_16MHz[static_cast<uint8_t>(bitrate)][0]),
    _intPin(intPin) {
}

void Mcp2515::initialInTask() {
    for (int i = 0; i < 2; i++) {
        if (txSemaphore[i] == nullptr) {
            txSemaphore[i] = xSemaphoreCreateBinary();
            canNum = i;
            break;
        } else if (i == 1) {
            // Should not happen, only 2 CAN interfaces supported
            assert(false);
        }
    }

    resetMcp2515();
    vTaskDelay(pdMS_TO_TICKS(100));  // Wait for the MCP2515 to reset
    setMcp2515Bitrate();
    clearInterrupts(0);

    gpio_init(_intPin);
    gpio_set_pulls(_intPin, true, false);
    gpio_set_irq_enabled_with_callback(_intPin, GPIO_IRQ_EDGE_FALL, true,
        canNum ? irqCallback1 : irqCallback0);
    enableInterrupts(TX0IE);
    writeByte(CANCTRL, REQOP_NORMAL | CLKOUT_ENABLED);
}

void Mcp2515::txBuffer0SendInTask(uint32_t canId, uint8_t* buf, uint8_t len) {
    txBuffer0Send(canId, buf, len);

    uint8_t intFlag = 0;
    do {
        xSemaphoreTake(txSemaphore[canNum], portMAX_DELAY);
        intFlag = readByte(CANINTF);
    } while (intFlag & TX0IF == 0);
    clearInterrupts(intFlag & ~(TX0IF | intFlag));
}

void Mcp2515::resetMcp2515() {
    uint8_t resetCmd[2] = { CAN_WRITE, CAN_RESET };  // MCP2515 RESET command
    _spi->csSelect();
    _spi->transfer(resetCmd, nullptr, 2);
    _spi->csDeselect();
}

void Mcp2515::setMcp2515Bitrate() {
    writeByte(CNF1, bitrateAddr[0]);
    writeByte(CNF2, bitrateAddr[1]);
    writeByte(CNF3, bitrateAddr[2]);
}

void Mcp2515::clearInterrupts(uint8_t interruptMask) {
    writeByte(CANINTF, interruptMask);
}

void Mcp2515::enableInterrupts(uint8_t interruptMask) {
    writeByte(CANINTE, interruptMask);
}

uint8_t Mcp2515::readByte(uint8_t Addr) {
    uint8_t cmd[2] = { CAN_READ, Addr };
    uint8_t rdata = 0;
    _spi->csSelect();
    _spi->transfer(cmd, nullptr, 2);  // Send read command
    _spi->transfer(nullptr, &rdata, 1);  // Read data byte
    _spi->csDeselect();
    return rdata;
}

void Mcp2515::writeByte(uint8_t Addr, uint8_t data) {
    _spi->csSelect();
    uint8_t cmd[3] = { CAN_WRITE, Addr, data };
    _spi->transfer(cmd, nullptr, 3);
    _spi->csDeselect();
}

void Mcp2515::txBuffer0Send(uint32_t canId, uint8_t* buf, uint8_t len) {
    if (canId > 0x7FF) {
        writeByte(TXB0EID0, canId & 0xFF);
        writeByte(TXB0EID8, (canId >> 8) & 0xFF);
        writeByte(TXB0SIDL, 0x08 | ((canId >> 16) & 0x3) | (((canId >> 18) & 0x07) << 5));
        writeByte(TXB0SIDH, (canId >> 21) & 0XFF);
    } else {
        writeByte(TXB0SIDH, (canId >> 3) & 0XFF);
        writeByte(TXB0SIDL, (canId & 0x07) << 5);
    }
    writeByte(TXB0DLC, len);

    for (uint8_t j = 0; j < len; j++) {
        writeByte(TXB0D0 + j, buf[j]);
    }
    writeByte(TXB0CTRL, 0x08);
}

void irqCallback0(uint gpio, uint32_t events) {
    if (txSemaphore[0] == nullptr)
        return;
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(txSemaphore[0], &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}

void irqCallback1(uint gpio, uint32_t events) {
    if (txSemaphore[1] == nullptr)
        return;
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(txSemaphore[1], &higherPriorityTaskWoken);
    portYIELD_FROM_ISR(higherPriorityTaskWoken);
}
