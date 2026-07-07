#ifndef _MCP2515_H_
#define _MCP2515_H_

#include "spi.h"

class Mcp2515 {
 public:
    typedef enum class Oscillator : uint8_t {
        MHZ8 = 0,
        MHZ16
    } Oscillator;
    typedef enum class Bitrate : uint8_t {
        KBPS5 = 0,
        KBPS10,
        KBPS20,
        KBPS50,
        KBPS100,
        KBPS125,
        KBPS250,
        KBPS500,
        KBPS1000
    } Bitrate;
    typedef enum class RxBufferType : uint8_t {
        Rollover,
        Independent
    } RxBufferType;

    Mcp2515(SPI* spi, uint32_t intPin, Oscillator oscillator, Bitrate bitrate);
    virtual ~Mcp2515() {}
    void txBuffer0Send(uint32_t canId, uint8_t* buf, uint8_t len);
    // filterId: 0-1 → RXB0, 2-5 → RXB1. maskId: 0 → RXB0, 1 → RXB1.
    // Filters only take effect in Independent mode (RXM_VALID_ALL).
    void setFilter(uint8_t filterId, uint32_t canId, bool extended = false);
    void setMask(uint8_t maskId, uint32_t mask, bool extended = false);

 protected:
    SPI* _spi;
    uint8_t* bitrateAddr;
    int32_t _intPin;
    void resetMcp2515();
    void setMcp2515Bitrate();
    void clearInterrupts(uint8_t interruptMask);
    void enableInterrupts(uint8_t interruptMask);
    uint8_t readByte(uint8_t Addr);
    void writeByte(uint8_t Addr, uint8_t data);
    void rxBuffer0Read(uint32_t* canId, uint8_t* buf, uint8_t* len);
    void rxBuffer1Read(uint32_t* canId, uint8_t* buf, uint8_t* len);
    void writeFilterOrMask(uint8_t sidh, uint32_t id, bool extended);
    void bitModify(uint8_t addr, uint8_t mask, uint8_t data);

 private:
    uint8_t bitrateToReg_8MHz[9][3] = {
        {0xA7, 0XBF, 0x07},  // KBPS5
        {0x31, 0XA4, 0X04},  // KBPS10
        {0x18, 0XA4, 0x04},  // KBPS20
        {0x09, 0XA4, 0x04},  // KBPS50
        {0x04, 0x9E, 0x03},  // KBPS100
        {0x03, 0x9E, 0x03},  // KBPS125
        {0x01, 0x1E, 0x03},  // KBPS250
        {0x00, 0x9E, 0x03},  // KBPS500
        {0x00, 0x82, 0x02}   // KBPS1000
    };
    uint8_t bitrateToReg_16MHz[9][3] = {
        {0x3F, 0xFF, 0x87},  // KBPS5
        {0x1F, 0xFF, 0x87},  // KBPS10
        {0x0F, 0xFF, 0x87},  // KBPS20
        {0x07, 0xFA, 0x87},  // KBPS50
        {0x03, 0xFA, 0x87},  // KBPS100
        {0x03, 0xF0, 0x86},  // KBPS125
        {0x41, 0xF1, 0x85},  // KBPS250
        {0x00, 0xF0, 0x86},  // KBPS500
        {0x00, 0xD0, 0x82}   // KBPS1000
    };
};

#endif
