#include "microkernel/core/task_context.h"

namespace jixia::microkernel::task {
namespace {

constexpr uintptr_t kMstatusMppMask = uintptr_t{3U} << 11U;
constexpr uintptr_t kMstatusMpie = uintptr_t{1U} << 7U;
constexpr uintptr_t kMstatusMprv = uintptr_t{1U} << 17U;

[[nodiscard]] uintptr_t read_mstatus() {
    uintptr_t value = 0U;
    __asm__ volatile("csrr %0, mstatus" : "=r"(value));
    return value;
}

[[nodiscard]] uintptr_t read_satp() {
    uintptr_t value = 0U;
    __asm__ volatile("csrr %0, satp" : "=r"(value));
    return value;
}

} // namespace

void clear(TaskContext& context) {
    for (uintptr_t& value : context.x) {
        value = 0U;
    }

    context.mstatus = 0U;
    context.mepc = 0U;
    context.satp = 0U;
}

void initialize_user(TaskContext& context, uintptr_t entry, uintptr_t argument, uintptr_t stack_top,
                     uintptr_t return_stub, uint64_t satp) {
    clear(context);

    context.x[1] = return_stub;
    context.x[2] = stack_top;
    context.x[10] = argument;

    /* mret must enter U-mode and must not borrow U translation via MPRV. */
    context.mstatus = (read_mstatus() & ~(kMstatusMppMask | kMstatusMprv)) | kMstatusMpie;
    context.mepc = entry;
    context.satp = satp;
}

void save_from_trap(TaskContext& context, const jixia::arch::riscv::TrapFrame& frame) {
    for (size_t index = 0U; index < 32U; ++index) {
        context.x[index] = frame.x[index];
    }

    context.x[0] = 0U;
    context.mstatus = frame.mstatus;
    context.mepc = frame.mepc;
    context.satp = read_satp();
}

void restore_to_trap(const TaskContext& context, jixia::arch::riscv::TrapFrame& frame) {
    for (size_t index = 0U; index < 32U; ++index) {
        frame.x[index] = context.x[index];
    }

    frame.x[0] = 0U;
    frame.mstatus = context.mstatus;
    frame.mepc = context.mepc;
}

} // namespace jixia::microkernel::task
