#pragma once

#include <stdint.h>

#include "microkernel/core/singleton.h"

namespace jixia::microkernel::hart {
struct HartLocal;
}

namespace jixia::microkernel::time {

class TimeManager final {
  public:
    static TimeManager& instance();

    void initialize();
    void initialize_hart(hart::HartLocal& local) const;

    [[nodiscard]] uint64_t task_timeslice_ticks() const;
    [[nodiscard]] uint64_t idle_timeslice_ticks() const;

  private:
    friend class jixia::microkernel::Singleton<TimeManager>;

    TimeManager();

    uint64_t task_timeslice_ticks_;
    uint64_t idle_timeslice_ticks_;
};

} // namespace jixia::microkernel::time
