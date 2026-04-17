#ifndef _SPI_H_
#define _SPI_H_

#include "hardware/spi.h"
#include "dma.h"

#include "FreeRTOS.h"
#include "semphr.h"

class SPI {
public:
    SPI(spi_inst_t* spi, uint32_t baudrate, uint32_t cs, uint32_t sck,
    uint32_t mosi, uint32_t miso,
    bool data16Bits, spi_cpol_t cpol, spi_cpha_t cpha,
    spi_order_t order, bool enableDma);
    virtual ~SPI() {}

    void csSelect();
    void csDeselect();
    uint16_t transfer(void* tx, void* rx, uint16_t size, uint32_t timeout_ms = portMAX_DELAY);

    void* operator new(size_t size)     {return pvPortMalloc(size);}
    void* operator new[](size_t size)   {return pvPortMalloc(size);}
    void operator delete(void * ptr)    {vPortFree(ptr);}
    void operator delete[](void * ptr)  {vPortFree(ptr);}

private:
    spi_inst_t* _spi;
    uint32_t _cs;
    bool _enableDma;
    uint32_t dmaTx;
    dma_channel_config dmaTxConfig;
    uint32_t dmaRx;
    dma_channel_config dmaRxConfig;
    bool _data16Bits;

    SemaphoreHandle_t dmaRxSemaphore;
    SemaphoreHandle_t dmaTxSemaphore;

    static void damCallback(void* param);

    void setFormat(uint data_bits, spi_cpol_t cpol, spi_cpha_t cpha, spi_order_t order);
    void dmaConfigure();
    void dmaWrite(void* data, uint16_t size);
    void dmaRead(void* data, uint16_t size);
    void dmaStart();
};

#endif
