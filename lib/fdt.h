#pragma once

#include <stdint.h>


namespace jixia::fdt {


struct CpuCountResult
{
    uint32_t count;
    bool valid;
};


[[nodiscard]]
CpuCountResult cpu_count(uintptr_t dtb_address);


} // namespace jixia::fdt