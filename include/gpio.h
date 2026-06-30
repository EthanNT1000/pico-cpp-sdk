#pragma once
#include "hardware/gpio.h"
#include <functional>

using GpioIrqCallback = std::function<void(uint gpio, uint32_t events)>;

// Register a per-pin IRQ callback and enable the specified events.
// All GPIO IRQ users must go through here so only one global callback is installed.
// Use gpio_set_irq_enabled() directly to enable/disable events after registration.
void gpioRegisterIrqCallback(uint pin, uint32_t events, GpioIrqCallback callback);
void gpioUnregisterIrqCallback(uint pin);
