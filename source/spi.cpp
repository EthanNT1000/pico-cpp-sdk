#include "spi.h"
#include "pico/cyw43_arch.h"

SPI::SPI(spi_inst_t* spi, uint32_t baudrate, uint32_t cs, uint32_t sck,
    uint32_t mosi, uint32_t miso,
    bool data16Bits, spi_cpol_t cpol, spi_cpha_t cpha,
    spi_order_t order, bool enableDma) : _spi(spi), _cs(cs),
    _data16Bits(data16Bits), _enableDma(enableDma) {
    spi_init(spi, baudrate);
    gpio_set_function(cs, GPIO_FUNC_SIO);
    gpio_set_function(sck, GPIO_FUNC_SPI);
    gpio_set_function(mosi, GPIO_FUNC_SPI);
    gpio_set_function(miso, GPIO_FUNC_SPI);

    gpio_set_dir(cs, GPIO_OUT);
    csDeselect();

    setFormat(data16Bits ? 16 : 8, cpol, cpha, order);
    if (enableDma) {
        dmaRxSemaphore = xSemaphoreCreateBinary();
        dmaTxSemaphore = xSemaphoreCreateBinary();
        dmaRx = DMA::enableDma(damCallback, dmaRxSemaphore);
        dmaTx = DMA::enableDma(damCallback, dmaTxSemaphore);

        dmaConfigure();
    }
}

void SPI::setFormat(uint data_bits, spi_cpol_t cpol, spi_cpha_t cpha, spi_order_t order) {
    spi_set_format(_spi, data_bits, cpol, cpha, order);
}

void SPI::csSelect() {
    gpio_put(_cs, false);   // Active low
}

void SPI::csDeselect() {
    gpio_put(_cs, true);
}

uint16_t SPI::transfer(void* tx, void* rx, uint16_t size, uint32_t timeout_ms) {
    if (_enableDma) {
        dmaWrite(tx == nullptr ? rx : tx, size);
        dmaRead(rx == nullptr ? tx : rx, size);
        dmaStart();
        xSemaphoreTake(dmaTxSemaphore, pdMS_TO_TICKS(timeout_ms));
        xSemaphoreTake(dmaRxSemaphore, pdMS_TO_TICKS(timeout_ms));
        return size;
    } else if (_data16Bits) {
        return spi_write16_read16_blocking(_spi, reinterpret_cast<uint16_t*>(tx),
            reinterpret_cast<uint16_t*>(rx), size);
    } else {
        return spi_write_read_blocking(_spi, reinterpret_cast<uint8_t*>(tx), reinterpret_cast<uint8_t*>(rx), size);
    }
}

void SPI::dmaConfigure() {
    dmaTxConfig = dma_channel_get_default_config(dmaTx);
    channel_config_set_transfer_data_size(&dmaTxConfig, _data16Bits ? DMA_SIZE_16 : DMA_SIZE_8);
    channel_config_set_dreq(&dmaTxConfig, spi_get_dreq(_spi, true));
    dma_channel_configure(dmaTx, &dmaTxConfig,
        &spi_get_hw(_spi)->dr,  // write address
        nullptr,  // read address
        0,
        false);

    dmaRxConfig = dma_channel_get_default_config(dmaRx);
    channel_config_set_transfer_data_size(&dmaRxConfig, _data16Bits ? DMA_SIZE_16 : DMA_SIZE_8);
    channel_config_set_dreq(&dmaRxConfig, spi_get_dreq(_spi, false));
    channel_config_set_read_increment(&dmaRxConfig, false);
    channel_config_set_write_increment(&dmaRxConfig, true);
    dma_channel_configure(dmaRx, &dmaRxConfig,
        nullptr,  // write address
        &spi_get_hw(_spi)->dr,  // read address
        0,
        false);
}

void SPI::dmaWrite(void* data, uint16_t size) {
    dma_channel_set_read_addr(dmaTx, data, false);
    dma_channel_set_transfer_count(dmaTx, size, false);
}

void SPI::dmaRead(void* data, uint16_t size) {
    // Give the channel a new wave table entry to read from, and re-trigger it
    dma_channel_set_write_addr(dmaRx, data, false);
    dma_channel_set_transfer_count(dmaRx, size, false);
}

void SPI::dmaStart() {
    dma_start_channel_mask((1u << dmaTx) | (1u << dmaRx));
}

void SPI::damCallback(void* param) {
    BaseType_t xHigherPriorityTaskWoken;
    xSemaphoreGiveFromISR(reinterpret_cast<SemaphoreHandle_t>(param), &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}
