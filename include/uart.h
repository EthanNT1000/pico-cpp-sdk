#ifndef _UART_H_
#define _UART_H_

#include "hardware/uart.h"
#include "dma.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "timers.h"

class UART {
public:
    // rxBuffer must be 2 * rxHalfSize bytes.
    // idleTimeoutMs: how long to wait with no new DMA completion before reporting partial data.
    UART(uart_inst_t* uart, uint32_t baudrate, uint32_t tx, uint32_t rx,
         bool enableTxDma, bool enableRxDma,
         uint8_t* rxBuffer = nullptr, uint16_t rxHalfSize = 0,
         uint32_t idleTimeoutMs = 5);
    virtual ~UART() {}

    uint16_t write(const void* data, uint16_t size, uint32_t timeout_ms = portMAX_DELAY);

    // Blocks until data is ready (full half OR idle timeout with partial data).
    // Returns pointer into rx buffer and sets *size to actual bytes received.
    const uint8_t* read(uint16_t* size, uint32_t timeout_ms = portMAX_DELAY);

    // Blocking read into caller buffer. Use when enableRxDma=false.
    uint16_t readBlocking(void* data, uint16_t size);

    void* operator new(size_t size)    { return pvPortMalloc(size); }
    void* operator new[](size_t size)  { return pvPortMalloc(size); }
    void operator delete(void* ptr)    { vPortFree(ptr); }
    void operator delete[](void* ptr)  { vPortFree(ptr); }

private:
    uart_inst_t* _uart;
    bool _enableTxDma;
    bool _enableRxDma;

    uint32_t dmaTx;
    dma_channel_config dmaTxConfig;
    SemaphoreHandle_t dmaTxSemaphore;

    uint32_t dmaRx;
    dma_channel_config dmaRxConfig;
    SemaphoreHandle_t dmaRxSemaphore;
    uint8_t* _rxBuffer;
    uint16_t _rxHalfSize;
    volatile uint8_t  _rxActiveHalf;
    volatile uint16_t _rxBytesReady;   // set before giving semaphore
    volatile bool     _rxDmaActive;    // true while DMA is running

    TimerHandle_t _rxIdleTimer;
    uint32_t _idleTimeoutMs;

    // One instance per UART peripheral for IRQ dispatch
    static UART* _instances[2];

    static void dmaTxCallback(void* param);
    static void dmaRxCallback(void* param);
    static void rxIdleTimerCallback(TimerHandle_t timer);
    static void uart0IrqHandler();
    static void uart1IrqHandler();
    static void uartIrqCommon(UART* self);

    void dmaTxConfigure();
    void dmaRxConfigure();
    void dmaWrite(const void* data, uint16_t size);
    void startRxDma();
    void enableRxIrq();
    void disableRxIrq();
    uint16_t rxBytesReceived() const;
};

#endif
