#ifndef _DMA_H_
#define _DMA_H_

#include "hardware/dma.h"

#include "FreeRTOS.h"

class DMA {
 public:
    typedef void (*InterruptCallback)(void* param);
    static int32_t enableDma(InterruptCallback callback = nullptr, void* param = nullptr);
    static void resetCallbackList();

    void* operator new(size_t size)     {return pvPortMalloc(size);}
    void* operator new[](size_t size)   {return pvPortMalloc(size);}
    void operator delete(void * ptr)    {vPortFree(ptr);}
    void operator delete[](void * ptr)  {vPortFree(ptr);}

 private:
    typedef struct {
        InterruptCallback callback;
        void* param;
    } Callback_t;

    static Callback_t callbackList[NUM_DMA_CHANNELS];
    static constexpr uint8_t dmaIrq1channelStart = NUM_DMA_CHANNELS / 2;
    static void dma_handler0();
    static void dma_handler1();
};

#endif
