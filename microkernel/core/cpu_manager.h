#pragma once

#include "microkernel/core/hart.h"
#include "microkernel/core/singleton.h"

namespace jixia::microkernel::cpu {

class CpuManager final {
  public:
    static CpuManager& instance();

    [[nodiscard]] bool initialize(hart::HartIndex present_count);

    [[nodiscard]] hart::HartLocal& current() const;
    [[nodiscard]] hart::HartLocal& boot_hart() const;
    [[nodiscard]] hart::HartIndex present_count() const;

  private:
    friend class jixia::microkernel::Singleton<CpuManager>;

    CpuManager();

    hart::HartIndex present_count_;
};

} // namespace jixia::microkernel::cpu
