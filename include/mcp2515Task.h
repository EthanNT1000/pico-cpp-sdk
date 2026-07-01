#pragma once
#include "mcp2515.h"
#include "taskinterface.h"

class Mcp2515Task : public Mcp2515, public TaskInterface {
 public:
     Mcp2515Task(SPI* spi, uint32_t intPin, Mcp2515::Oscillator oscillator, Mcp2515::Bitrate bitrate,
         TaskInterface::Priority priority = TaskInterface::High,
         configSTACK_DEPTH_TYPE stackDepth = 512);

    void initialInTask(RxBufferType rxBufferType = RxBufferType::Rollover);
    void txBuffer0SendInTask(uint32_t canId, uint8_t* buf, uint8_t len);
    void rxReadInTask(uint32_t* canId, uint8_t* buf, uint8_t* len);

    void* operator new(size_t size)   { return pvPortMalloc(size); }
    void* operator new[](size_t size) { return pvPortMalloc(size); }
    void operator delete(void* ptr)   { vPortFree(ptr); }
    void operator delete[](void* ptr) { vPortFree(ptr); }

 protected:
    void run() override;  // canService: dispatches intSemaphore → rx/tx semaphores

 private:
    void irqHandler(uint gpio, uint32_t events);

    SemaphoreHandle_t _intSemaphore { nullptr };  // given by ISR
    SemaphoreHandle_t _rxSemaphore  { nullptr };  // counting, given once per received frame
    SemaphoreHandle_t _txSemaphore  { nullptr };  // given by canService on TX complete
    volatile uint8_t  _pendingRxBuf { 0 };        // bitmask: bit0=RXB0 ready, bit1=RXB1 ready
    volatile uint8_t  _pendingTxBuf { 0 };        // bitmask: bit0=TXB0 ready, bit1=TXB1 ready, bit2=TXB2 ready
};
