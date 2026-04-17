#include "dma.h"
#include <string.h>
#include "pico/cyw43_arch.h"

DMA::Callback_t DMA::callbackList[NUM_DMA_CHANNELS];

void DMA::dma_handler0() {
    for (uint8_t c = 0; c < dmaIrq1channelStart; c++) {
        if (dma_channel_get_irq0_status(c) && callbackList[c].callback != nullptr) {
            callbackList[c].callback(callbackList[c].param);
            dma_channel_acknowledge_irq0(c);
        }
    }
}

void DMA::dma_handler1() {
    for (uint8_t c = dmaIrq1channelStart; c < NUM_DMA_CHANNELS; c++) {
        if (dma_channel_get_irq0_status(c) && callbackList[c].callback != nullptr) {
            callbackList[c].callback(callbackList[c].param);
            dma_channel_acknowledge_irq0(c);
        }
    }
}

void DMA::resetCallbackList() {
    memset(callbackList, 0, sizeof(callbackList));
}

int32_t DMA::enableDma(InterruptCallback callback, void* param) {
    int32_t channel = dma_claim_unused_channel(true);

    // Tell the DMA to raise IRQ line 0 when the channel finishes a block
    if (callback != nullptr) {
        callbackList[channel].callback = callback;
        callbackList[channel].param = param;

        if (channel < dmaIrq1channelStart) {
            dma_channel_set_irq0_enabled(channel, true);
            irq_set_exclusive_handler(DMA_IRQ_0, DMA::dma_handler0);
            irq_set_enabled(DMA_IRQ_0, true);
        } else {
            dma_channel_set_irq1_enabled(channel, true);
            irq_set_exclusive_handler(DMA_IRQ_1, DMA::dma_handler1);
            irq_set_enabled(DMA_IRQ_1, true);
        }
    }
    return channel;
}
