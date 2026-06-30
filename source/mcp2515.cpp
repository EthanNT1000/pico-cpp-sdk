#include "mcp2515.h"
#include "mcp2515reg.h"
#include "pico/stdlib.h"

Mcp2515::Mcp2515(SPI* spi, uint32_t intPin, Oscillator oscillator, Bitrate bitrate) :
    _spi(spi), bitrateAddr(oscillator == Oscillator::MHZ8 ?
        &bitrateToReg_8MHz[static_cast<uint8_t>(bitrate)][0] :
        &bitrateToReg_16MHz[static_cast<uint8_t>(bitrate)][0]),
    _intPin(intPin) {
}

void Mcp2515::resetMcp2515() {
    uint8_t resetCmd[2] = { CAN_WRITE, CAN_RESET };
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
    _spi->transfer(cmd, nullptr, 2);
    _spi->transfer(nullptr, &rdata, 1);
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
    for (uint8_t j = 0; j < len; j++) writeByte(TXB0D0 + j, buf[j]);
    writeByte(TXB0CTRL, 0x08);
}

void Mcp2515::rxBuffer0Read(uint32_t* canId, uint8_t* buf, uint8_t* len) {
    uint8_t sidh = readByte(RXB0SIDH);
    uint8_t sidl = readByte(RXB0SIDL);
    uint8_t dlc  = readByte(RXB0DLC) & 0x0F;
    if (sidl & EXIDE_SET) {
        uint8_t eid8 = readByte(RXB0EID8);
        uint8_t eid0 = readByte(RXB0EID0);
        *canId = ((uint32_t)sidh << 21) | ((uint32_t)(sidl >> 5) << 18) |
                 ((uint32_t)(sidl & 0x03) << 16) | ((uint32_t)eid8 << 8) | eid0;
    } else {
        *canId = ((uint32_t)sidh << 3) | (sidl >> 5);
    }
    *len = dlc;
    for (uint8_t i = 0; i < dlc; i++) buf[i] = readByte(RXB0D0 + i);
}

void Mcp2515::rxBuffer1Read(uint32_t* canId, uint8_t* buf, uint8_t* len) {
    uint8_t sidh = readByte(RXB1SIDH);
    uint8_t sidl = readByte(RXB1SIDL);
    uint8_t dlc  = readByte(RXB1DLC) & 0x0F;
    if (sidl & EXIDE_SET) {
        uint8_t eid8 = readByte(RXB1EID8);
        uint8_t eid0 = readByte(RXB1EID0);
        *canId = ((uint32_t)sidh << 21) | ((uint32_t)(sidl >> 5) << 18) |
                 ((uint32_t)(sidl & 0x03) << 16) | ((uint32_t)eid8 << 8) | eid0;
    } else {
        *canId = ((uint32_t)sidh << 3) | (sidl >> 5);
    }
    *len = dlc;
    for (uint8_t i = 0; i < dlc; i++) buf[i] = readByte(RXB1D0 + i);
}

void Mcp2515::bitModify(uint8_t addr, uint8_t mask, uint8_t data) {
    _spi->csSelect();
    uint8_t cmd[4] = { CAN_BIT_MODIFY, addr, mask, data };
    _spi->transfer(cmd, nullptr, 4);
    _spi->csDeselect();
}

void Mcp2515::writeFilterOrMask(uint8_t sidh, uint32_t id, bool extended) {
    if (extended) {
        writeByte(sidh,     (id >> 21) & 0xFF);
        writeByte(sidh + 1, EXIDE_SET | ((id >> 18) & 0x07) << 5 | (id >> 16) & 0x03);
        writeByte(sidh + 2, (id >> 8) & 0xFF);
        writeByte(sidh + 3, id & 0xFF);
    } else {
        writeByte(sidh,     (id >> 3) & 0xFF);
        writeByte(sidh + 1, (id & 0x07) << 5);
        writeByte(sidh + 2, 0x00);
        writeByte(sidh + 3, 0x00);
    }
}

void Mcp2515::setFilter(uint8_t filterId, uint32_t canId, bool extended) {
    const uint8_t filterSidh[] = { RXF0SIDH, RXF1SIDH, RXF2SIDH, RXF3SIDH, RXF4SIDH, RXF5SIDH };
    if (filterId >= 6) return;
    uint8_t rxbCtrl = (filterId < 2) ? RXB0CTRL : RXB1CTRL;
    writeByte(CANCTRL, REQOP_CONFIG);
    writeFilterOrMask(filterSidh[filterId], canId, extended);
    writeByte(CANCTRL, REQOP_NORMAL | CLKOUT_ENABLED);
    writeByte(rxbCtrl, readByte(rxbCtrl) & ~RXM);
}

void Mcp2515::setMask(uint8_t maskId, uint32_t mask, bool extended) {
    const uint8_t maskSidh[] = { RXM0SIDH, RXM1SIDH };
    const uint8_t rxbCtrl[] = { RXB0CTRL, RXB1CTRL };
    if (maskId >= 2) return;
    writeByte(CANCTRL, REQOP_CONFIG);
    writeFilterOrMask(maskSidh[maskId], mask, extended);
    writeByte(CANCTRL, REQOP_NORMAL | CLKOUT_ENABLED);
    writeByte(rxbCtrl[maskId], readByte(rxbCtrl[maskId]) & ~RXM);
}
