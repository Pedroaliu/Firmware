#include "microkernel/core/heap_manager.h"

#include "microkernel/core/singleton.h"

namespace jixia::microkernel::memory {

HeapManager::HeapManager() : initialized_(false) {
}

HeapManager& HeapManager::instance() {
    return Singleton<HeapManager>::instance();
}

void HeapManager::initialize() {
    initialized_ = true;
}

bool HeapManager::dynamic_allocation_available() const {
    /* M00-08 bootstrap tasks use a bounded fixed pool until heap allocation. */
    return false;
}

} // namespace jixia::microkernel::memory
