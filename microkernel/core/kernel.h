#pragma once

#include "microkernel/core/hart.h"
#include "microkernel/core/singleton.h"

namespace jixia::microkernel {

class Kernel final {
  public:
    static Kernel& instance();

    [[noreturn]] void bootstrap(hart::HartIndex present_count);
    [[noreturn]] void secondary_bootstrap();

  private:
    friend class Singleton<Kernel>;

    Kernel();

    void cpp_bootstrap();
    [[nodiscard]] bool boot_data_bootstrap();
    [[nodiscard]] bool memory_bootstrap();
    [[nodiscard]] bool cpu_bootstrap(hart::HartIndex present_count);
    void ipc_bootstrap();
    void platform_status_bootstrap();
    void debug_bootstrap();
    [[nodiscard]] bool init_task_bootstrap();
    void deferred_bootstrap();
    [[noreturn]] void dispatch_task();

    bool cpp_ready_;
};

} // namespace jixia::microkernel
