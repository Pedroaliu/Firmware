#include "microkernel/core/task_context.h"

namespace jixia::microkernel::task {
namespace {

using jixia::arch::riscv::Xlen;

constexpr uintptr_t kUserStackAlignment = 16U;

constexpr Xlen kMstatusMpie = Xlen{1U} << 7U;
constexpr Xlen kMstatusUxlRv64 = Xlen{2U} << 32U;
constexpr Xlen kMstatusSxlRv64 = Xlen{2U} << 34U;

constexpr Xlen kInitialUserMstatus = kMstatusMpie | kMstatusUxlRv64 | kMstatusSxlRv64;

} // namespace

bool initialize(TaskContext& context, TaskId id, const VSpace& vspace, const UserTaskStart& start) {
    if (!vspace.address_space.valid() || start.entry_pc == 0U || (start.entry_pc & 0x1U) != 0U ||
        start.stack_pointer == 0U || (start.stack_pointer & (kUserStackAlignment - 1U)) != 0U) {
        return false;
    }

    context = {};

    context.id = id;
    context.state = TaskState::ready;
    context.vspace = vspace;

    context.user_frame.mstatus = kInitialUserMstatus;
    context.user_frame.mepc = start.entry_pc;
    context.user_frame.x[2] = start.stack_pointer;
    context.user_frame.x[3] = start.global_pointer;
    context.user_frame.x[10] = start.argument;

    return true;
}

} // namespace jixia::microkernel::task