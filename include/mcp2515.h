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

    Mcp2515(SPI* spi, uint32_t intPin, Oscillator oscillator, Bitrate bitrate);
    virtual ~Mcp2515() {}
    void initialInTask();
    void txBuffer0SendInTask(uint32_t canId, uint8_t* buf, uint8_t len);
    void txBuffer0Send(uint32_t canId, uint8_t* buf, uint8_t len);

    void* operator new(size_t size) { return pvPortMalloc(size); }
    void* operator new[](size_t size) { return pvPortMalloc(size); }
    void operator delete(void* ptr) { vPortFree(ptr); }
    void operator delete[](void* ptr) { vPortFree(ptr); }

 private:
    uint8_t bitrateToReg_8MHz[9][3] = {
        {0xA7, 0XBF, 0x07},  // KBPS5    (5 kbps)
        {0x31, 0XA4, 0X04},  // KBPS10   (10 kbps)
        {0x18, 0XA4, 0x04},  // KBPS20   (20 kbps)
        {0x09, 0XA4, 0x04},  // KBPS50   (50 kbps)
        {0x04, 0x9E, 0x03},  // KBPS100  (100 kbps)
        {0x03, 0x9E, 0x03},  // KBPS125  (125 kbps)
        {0x01, 0x1E, 0x03},  // KBPS250  (250 kbps)
        {0x00, 0x9E, 0x03},  // KBPS500  (500 kbps)
        {0x00, 0x82, 0x02}   // KBPS1000 (1000 kbps)
    };

    // ref: https://github.com/p1ne/arduino-can-bus-library/blob/master/mcp_can_dfs.h
    uint8_t bitrateToReg_16MHz[9][3] = {
        {0x3F, 0xFF, 0x87},  // KBPS5    (5 kbps)
        {0x1F, 0xFF, 0x87},  // KBPS10   (10 kbps)
        {0x0F, 0xFF, 0x87},  // KBPS20   (20 kbps)
        {0x07, 0xFA, 0x87},  // KBPS50   (50 kbps)
        {0x03, 0xFA, 0x87},  // KBPS100  (100 kbps)
        {0x03, 0xF0, 0x86},  // KBPS125  (125 kbps)
        {0x41, 0xF1, 0x85},  // KBPS250  (250 kbps)
        {0x00, 0xF0, 0x86},  // KBPS500  (500 kbps)
        {0x00, 0xD0, 0x82}   // KBPS1000 (1000 kbps)
    };

    SPI* _spi;
    uint8_t* bitrateAddr;
    uint8_t canNum;
    int32_t _intPin;
    void resetMcp2515();
    void setMcp2515Bitrate();
    void clearInterrupts(uint8_t interruptMask);
    void enableInterrupts(uint8_t interruptMask);
    uint8_t readByte(uint8_t Addr);
    void writeByte(uint8_t Addr, uint8_t data);
};
#endif
