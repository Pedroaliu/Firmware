#include "platform/qemu_virt/console.h"

#include "microkernel/console/console.h"
#include "microkernel/console/sink.h"
#include "uart.h"

namespace jixia::platform::qemu_virt::console {
namespace {

void uart_put_char(void*, char ch)
{
    if (ch == '\n')
    {
        uart_putc('\r');
    }

    uart_putc(ch);
}

constinit jixia::microkernel::console::ConsoleSink uart_sink{
    nullptr,
    uart_put_char,
    nullptr,
    jixia::microkernel::console::SinkCapability::early_safe |
        jixia::microkernel::console::SinkCapability::panic_safe |
        jixia::microkernel::console::SinkCapability::blocking};

} // namespace

void initialize()
{
    jixia::microkernel::console::initialize();
    (void)jixia::microkernel::console::attach_sink(uart_sink);
}

} // namespace jixia::platform::qemu_virt::console
