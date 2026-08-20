#include "microkernel/core/ipc_manager.h"

namespace jixia::microkernel::ipc {

EndpointManager::EndpointManager() : table_lock_(), slot_allocated_(), endpoints_() {
}

EndpointManager& EndpointManager::instance() {
    return Singleton<EndpointManager>::instance();
}

bool EndpointManager::handle_well_formed(const EndpointHandle& handle) {
    return (static_cast<size_t>(handle.index) < kMaxEndpoints) && (handle.generation != 0U) &&
           (handle.generation <= kMaxGeneration);
}

void EndpointManager::clear_queue(Endpoint& endpoint) {
    for (size_t index = 0U; index < kEndpointQueueDepth; ++index) {
        endpoint.queue[index] = Message{};
    }
}

intptr_t EndpointManager::create_endpoint(TaskId owner, uint64_t* handle_out) {
    if (handle_out == nullptr) {
        return kErrorInvalidArgument;
    }

    /* Lock order: table lock -> endpoint lock (never the reverse). */
    SpinlockGuard table_guard(table_lock_);
    for (size_t index = 0U; index < kMaxEndpoints; ++index) {
        /* A retired slot is allocated forever: its epoch space is exhausted. */
        if (slot_allocated_[index] || endpoints_[index].retired) {
            continue;
        }

        Endpoint& endpoint = endpoints_[index];
        SpinlockGuard endpoint_guard(endpoint.lock);

        slot_allocated_[index] = true;
        endpoint.active = true;
        endpoint.retired = false;
        endpoint.owner = owner;
        endpoint.head = 0U;
        endpoint.count = 0U;
        clear_queue(endpoint);

        /*
         * Generation 0 is the never-allocated BSS state and never appears in a
         * live handle. A recycled slot keeps the epoch already advanced by its
         * destroy_endpoint call, so handles from the previous life fail closed
         * immediately; destroy owns the epoch increment.
         */
        if (endpoint.generation == 0U) {
            endpoint.generation = 1U;
        }

        *handle_out = EndpointHandle{static_cast<uint32_t>(index), endpoint.generation}.raw();
        return 0;
    }

    return kErrorNoSpace;
}

intptr_t EndpointManager::destroy_endpoint(TaskId caller, uint64_t handle) {
    const EndpointHandle parsed = EndpointHandle::from_raw(handle);
    if (!handle_well_formed(parsed)) {
        return kErrorInvalidArgument;
    }

    /* Lock order: table lock -> endpoint lock (never the reverse). */
    SpinlockGuard table_guard(table_lock_);
    Endpoint& endpoint = endpoints_[parsed.index];
    SpinlockGuard endpoint_guard(endpoint.lock);

    if (!slot_allocated_[parsed.index] || !endpoint.active ||
        (endpoint.generation != parsed.generation)) {
        return kErrorInvalidArgument;
    }

    if (endpoint.owner != caller) {
        return kErrorAccess;
    }

    /* Linearization point: DEAD flip, queue purge, epoch advance. */
    clear_queue(endpoint);
    endpoint.head = 0U;
    endpoint.count = 0U;
    endpoint.owner = 0U;
    endpoint.active = false;
    if (endpoint.generation >= kMaxGeneration) {
        /*
         * Ceiling reached: advancing further would set bit 63 of the next
         * handle. Retire the slot permanently instead of wrapping — an epoch
         * must never repeat, so a stale handle can never alias a new one.
         * slot_allocated_ stays true: the slot is never reallocated.
         */
        endpoint.retired = true;
    } else {
        endpoint.generation += 1U;
        slot_allocated_[parsed.index] = false;
    }
    return 0;
}

intptr_t EndpointManager::send(TaskId sender, uint64_t handle, const uint64_t (&words)[4]) {
    const EndpointHandle parsed = EndpointHandle::from_raw(handle);
    if (!handle_well_formed(parsed)) {
        return kErrorInvalidArgument;
    }

    /* Single endpoint lock only: no table, task, or time locks here. */
    Endpoint& endpoint = endpoints_[parsed.index];
    SpinlockGuard endpoint_guard(endpoint.lock);

    if (!endpoint.active || (endpoint.generation != parsed.generation)) {
        return kErrorInvalidArgument;
    }

    if (endpoint.count >= kEndpointQueueDepth) {
        return kErrorAgain;
    }

    Message& slot = endpoint.queue[(endpoint.head + endpoint.count) % kEndpointQueueDepth];
    slot.sender = sender;
    slot.words[0] = words[0];
    slot.words[1] = words[1];
    slot.words[2] = words[2];
    slot.words[3] = words[3];
    endpoint.count += 1U;
    return 0;
}

intptr_t EndpointManager::try_recv(uint64_t handle, Message* message_out) {
    const EndpointHandle parsed = EndpointHandle::from_raw(handle);
    if (!handle_well_formed(parsed) || (message_out == nullptr)) {
        return kErrorInvalidArgument;
    }

    /* Single endpoint lock only: no table, task, or time locks here. */
    Endpoint& endpoint = endpoints_[parsed.index];
    SpinlockGuard endpoint_guard(endpoint.lock);

    if (!endpoint.active || (endpoint.generation != parsed.generation)) {
        return kErrorInvalidArgument;
    }

    if (endpoint.count == 0U) {
        return kErrorAgain;
    }

    const Message& slot = endpoint.queue[endpoint.head];
    message_out->sender = slot.sender;
    message_out->words[0] = slot.words[0];
    message_out->words[1] = slot.words[1];
    message_out->words[2] = slot.words[2];
    message_out->words[3] = slot.words[3];

    endpoint.queue[endpoint.head] = {};
    endpoint.head = (endpoint.head + 1U) % kEndpointQueueDepth;
    endpoint.count -= 1U;
    return 0;
}

#ifdef JIXIA_M00_08_03_01_PROBE
bool EndpointManager::debug_probe_generation_ceiling() {
    constexpr TaskId kProbeOwner = 1U;

    EndpointManager& manager = instance();

    uint64_t scratch_raw = 0U;
    if (manager.create_endpoint(kProbeOwner, &scratch_raw) != 0) {
        return false;
    }
    const EndpointHandle scratch = EndpointHandle::from_raw(scratch_raw);
    if (scratch.generation != 1U) {
        return false;
    }

    /*
     * White-box: force the scratch slot to the generation ceiling. The live
     * handle now carries the ceiling epoch, exactly as an owner would hold
     * it after (forced) churn to the cap.
     */
    {
        SpinlockGuard table_guard(manager.table_lock_);
        SpinlockGuard endpoint_guard(manager.endpoints_[scratch.index].lock);
        manager.endpoints_[scratch.index].generation = kMaxGeneration;
    }
    const uint64_t ceiling_raw = EndpointHandle{scratch.index, kMaxGeneration}.raw();

    /* Destroy at the ceiling must retire instead of wrapping the epoch. */
    if (manager.destroy_endpoint(kProbeOwner, ceiling_raw) != 0) {
        return false;
    }

    /* The retired slot is never reallocated: create publishes another slot. */
    uint64_t second_raw = 0U;
    if (manager.create_endpoint(kProbeOwner, &second_raw) != 0) {
        return false;
    }
    const EndpointHandle second = EndpointHandle::from_raw(second_raw);
    if ((second.index == scratch.index) || (second.generation == 0U) ||
        (second.generation > kMaxGeneration)) {
        return false;
    }

    /* Retirement survives further churn on the neighboring slots. */
    if (manager.destroy_endpoint(kProbeOwner, second_raw) != 0) {
        return false;
    }
    uint64_t third_raw = 0U;
    if (manager.create_endpoint(kProbeOwner, &third_raw) != 0) {
        return false;
    }
    if (EndpointHandle::from_raw(third_raw).index == scratch.index) {
        return false;
    }
    if (manager.destroy_endpoint(kProbeOwner, third_raw) != 0) {
        return false;
    }

    /* Probe cleanup: restore pristine BSS-equivalent slot state. */
    {
        SpinlockGuard table_guard(manager.table_lock_);
        SpinlockGuard endpoint_guard(manager.endpoints_[scratch.index].lock);
        manager.endpoints_[scratch.index].generation = 0U;
        manager.endpoints_[scratch.index].retired = false;
        manager.slot_allocated_[scratch.index] = false;
    }
    return true;
}
#endif

} // namespace jixia::microkernel::ipc