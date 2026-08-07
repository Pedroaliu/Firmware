#pragma once

#include "microkernel/arch/riscv/isa.h"

namespace jixia::arch::riscv {

enum class ExceptionCode : Xlen
{
    instruction_address_misaligned = 0,
    instruction_access_fault = 1,
    illegal_instruction = 2,
    breakpoint = 3,
    load_address_misaligned = 4,
    load_access_fault = 5,
    store_address_misaligned = 6,
    store_access_fault = 7,
    environment_call_from_u = 8,
    environment_call_from_s = 9,
    environment_call_from_m = 11,
    instruction_page_fault = 12,
    load_page_fault = 13,
    store_page_fault = 15,
};

enum class InterruptCode : Xlen
{
    supervisor_software = 1,
    machine_software = 3,
    supervisor_timer = 5,
    machine_timer = 7,
    supervisor_external = 9,
    machine_external = 11,
};

class TrapCause
{
public:
    explicit constexpr TrapCause(Xlen raw)
        : raw_(raw)
    {
    }

    [[nodiscard]]
    constexpr Xlen raw() const
    {
        return raw_;
    }

    [[nodiscard]]
    constexpr bool is_interrupt() const
    {
        return (raw_ & mcause_interrupt_mask) != 0U;
    }

    [[nodiscard]]
    constexpr bool is_exception() const
    {
        return !is_interrupt();
    }

    [[nodiscard]]
    constexpr Xlen code() const
    {
        return raw_ & mcause_code_mask;
    }

    [[nodiscard]]
    constexpr bool is_exception(ExceptionCode expected) const
    {
        return is_exception() &&
            code() == static_cast<Xlen>(expected);
    }

    [[nodiscard]]
    constexpr bool is_interrupt(InterruptCode expected) const
    {
        return is_interrupt() &&
            code() == static_cast<Xlen>(expected);
    }

private:
    Xlen raw_;
};

static_assert(
    TrapCause{static_cast<Xlen>(ExceptionCode::breakpoint)}
        .is_exception(ExceptionCode::breakpoint));
static_assert(
    TrapCause{
        mcause_interrupt_mask |
        static_cast<Xlen>(InterruptCode::machine_software)}
        .is_interrupt(InterruptCode::machine_software));

} // namespace jixia::arch::riscv
