#pragma once

namespace jixia::microkernel {

/**
 * One lazily constructed executive object per firmware image.
 *
 * M00-08 constructs every executive singleton on the boot hart before other
 * harts are allowed to use it. The build deliberately disables hosted C++
 * thread-safe-static support, so that boot-order rule is part of the ABI.
 */
template <typename T> class Singleton final {
  public:
    [[nodiscard]] static T& instance() {
        static T value;
        return value;
    }

    Singleton() = delete;
};

} // namespace jixia::microkernel
