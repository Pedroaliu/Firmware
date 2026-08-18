#include "lib/memory.h"

#include <stdint.h>

extern "C" void* memset(void* destination, int value, size_t count) {
    auto* output = static_cast<volatile uint8_t*>(destination);
    const auto byte = static_cast<uint8_t>(value);

    for (size_t index = 0U; index < count; ++index) {
        output[index] = byte;
    }

    return destination;
}

extern "C" void* memcpy(void* destination, const void* source, size_t count) {
    auto* output = static_cast<volatile uint8_t*>(destination);
    const auto* input = static_cast<const volatile uint8_t*>(source);

    for (size_t index = 0U; index < count; ++index) {
        output[index] = input[index];
    }

    return destination;
}

extern "C" void* memmove(void* destination, const void* source, size_t count) {
    auto* output = static_cast<volatile uint8_t*>(destination);
    const auto* input = static_cast<const volatile uint8_t*>(source);
    const uintptr_t destination_address = reinterpret_cast<uintptr_t>(destination);
    const uintptr_t source_address = reinterpret_cast<uintptr_t>(source);

    if (destination_address <= source_address || destination_address - source_address >= count) {
        for (size_t index = 0U; index < count; ++index) {
            output[index] = input[index];
        }
    } else {
        for (size_t index = count; index > 0U; --index) {
            output[index - 1U] = input[index - 1U];
        }
    }

    return destination;
}

extern "C" int memcmp(const void* left, const void* right, size_t count) {
    const auto* left_bytes = static_cast<const volatile uint8_t*>(left);
    const auto* right_bytes = static_cast<const volatile uint8_t*>(right);

    for (size_t index = 0U; index < count; ++index) {
        const uint8_t left_byte = left_bytes[index];
        const uint8_t right_byte = right_bytes[index];
        if (left_byte != right_byte) {
            return static_cast<int>(left_byte) - static_cast<int>(right_byte);
        }
    }

    return 0;
}
