#include <stdint.h>

#include "microkernel/arch/riscv/privilege_boundary_test_values.h"
#include "microkernel/arch/riscv/trap_cause.h"
#include "microkernel/arch/riscv/trap_frame.h"
#include "microkernel/core/hart.h"

#ifdef JIXIA_M00_06_04_PROBE

extern "C" char jixia_m00_06_04_hostile_sp_ecall_site[];

namespace {

using jixia::arch::riscv::ExceptionCode;
using jixia::arch::riscv::TrapCause;
using jixia::arch::riscv::TrapFrame;
using jixia::arch::riscv::Xlen;

constexpr Xlen kMstatusMppShift = 11U;
constexpr Xlen kMstatusMppMask = 0x3U << kMstatusMppShift;
constexpr Xlen kMstatusMppSupervisor = 0x1U << kMstatusMppShift;

} // namespace

extern "C" bool jixia_m00_06_04_try_handle_supervisor_ecall(TrapFrame* frame)
{
    if (frame == nullptr)
    {
        return false;
    }

    const TrapCause cause{frame->mcause};
    if (!cause.is_exception(ExceptionCode::environment_call_from_s))
    {
        return false;
    }

    const uintptr_t expected_ecall_pc =
        reinterpret_cast<uintptr_t>(jixia_m00_06_04_hostile_sp_ecall_site);
    if (frame->mepc != expected_ecall_pc)
    {
        return false;
    }

    if ((frame->mstatus & kMstatusMppMask) != kMstatusMppSupervisor)
    {
        return false;
    }

    if (frame->x[2] != M00_06_04_HOSTILE_SP_MARKER ||
        frame->x[3] != M00_06_04_GP_MARKER || frame->x[10] != M00_06_04_A0_MARKER ||
        frame->x[17] != M00_06_04_A7_MARKER)
    {
        return false;
    }

    /*
     * The hostile x2 value is untrusted register state. The privileged frame
     * itself must be aligned and wholly contained in the current hart's
     * dedicated trusted M trap stack.
     */
    jixia::microkernel::hart::HartLocal& local = jixia::microkernel::hart::current();
    const uintptr_t frame_address = reinterpret_cast<uintptr_t>(frame);
    const uintptr_t frame_end = frame_address + sizeof(TrapFrame);

    if ((frame_address % TRAP_FRAME_ALIGNMENT) != 0U)
    {
        return false;
    }

    if (frame_address < local.trap_stack_bottom || frame_end > local.trap_stack_top)
    {
        return false;
    }

    if (local.trap_active != 1U)
    {
        return false;
    }

    /*
     * Validate first, then advance the first ECALL return PC. Before returning
     * to S, deliberately remove the trusted mscratch anchor so the second
     * ECALL exercises trap.S's lower-origin no-anchor fail-closed path.
     */
    frame->mepc += M00_06_04_ECALL_INSTRUCTION_BYTES;
    local.trap_active = 0U;
    __asm__ volatile("csrw mscratch, zero" ::: "memory");

    return true;
}

#endif
