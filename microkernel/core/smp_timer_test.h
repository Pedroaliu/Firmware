#pragma once

#include "microkernel/core/hart.h"


namespace jixia::microkernel::smp_timer_test {

void run_boot(hart::HartIndex present_count);
void run_secondary();

} // namespace jixia::microkernel::smp_timer_test
