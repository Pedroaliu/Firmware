#include "microkernel/console/kernel_console.h"

#include "uart.h"

namespace jixia::microkernel::kernel_console {
namespace {

char kernel_log_buffer[buffer_capacity];
size_t write_position = 0U;
bool was_truncated = false;
bool uart_mirror_enabled = false;

void mirror_to_uart(char ch)
{
    if (!uart_mirror_enabled)
    {
        return;
    }

    if (ch == '\n')
    {
        uart_putc('\r');
    }
    uart_putc(ch);
}

} // namespace

void set_uart_mirror(bool enabled)
{
    uart_mirror_enabled = enabled;
}

void put(char ch)
{
    if (write_position < buffer_capacity)
    {
        kernel_log_buffer[write_position] = ch;
        ++write_position;
    }
    else
    {
        was_truncated = true;
    }

    mirror_to_uart(ch);
}

void write(const char* text)
{
    if (text == nullptr)
    {
        text = "(null)";
    }

    while (*text != '\0')
    {
        put(*text);
        ++text;
    }
}

const char* buffer()
{
    return kernel_log_buffer;
}

size_t size()
{
    return write_position;
}

bool truncated()
{
    return was_truncated;
}

} // namespace jixia::microkernel::kernel_console
