#include <cstdlib>

#include "microkernel/core/hart.h"
#include "microkernel/core/scheduler.h"

/*
 * The hosted non-blocking torture binary links the production EndpointManager
 * but deliberately never creates firmware Tasks or enters blocking recv.
 * These fail-closed stubs satisfy the blocking-path symbols without importing
 * the target scheduler, trap, timer, or hart runtime into the host suite. Any
 * accidental hosted entry into a blocking kernel path terminates immediately.
 */
namespace jixia::microkernel::hart {

void park() {
    std::abort();
}

} // namespace jixia::microkernel::hart

namespace jixia::microkernel::scheduler {

bool Scheduler::add_task(task::Task&) {
    std::abort();
}

task::Task& Scheduler::set_next_runnable() {
    std::abort();
}

} // namespace jixia::microkernel::scheduler
