#pragma once

#include "microkernel/core/singleton.h"

namespace jixia::microkernel::memory {

class HeapManager final {
  public:
    static HeapManager& instance();

    void initialize();
    [[nodiscard]] bool dynamic_allocation_available() const;

  private:
    friend class jixia::microkernel::Singleton<HeapManager>;

    HeapManager();

    bool initialized_;
};

} // namespace jixia::microkernel::memory
