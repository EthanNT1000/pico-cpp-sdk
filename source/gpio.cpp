#include "gpio.h"
#include "hardware/irq.h"

static const uint kNumGpios = 30;   // RP2040 has GP0–GP29
static GpioIrqCallback pinCallbacks[kNumGpios];

static void irqDispatcher(uint gpio, uint32_t events) {
    if (gpio < kNumGpios && pinCallbacks[gpio])
        pinCallbacks[gpio](gpio, events);
}

void gpioRegisterIrqCallback(uint pin, uint32_t events, GpioIrqCallback callback) {
    if (pin >= kNumGpios) return;
    pinCallbacks[pin] = std::move(callback);
    gpio_set_irq_callback(irqDispatcher);
    irq_set_enabled(IO_IRQ_BANK0, true);
    gpio_set_irq_enabled(pin, events, true);
}

void gpioUnregisterIrqCallback(uint pin) {
    if (pin >= kNumGpios) return;
    pinCallbacks[pin] = nullptr;
}
