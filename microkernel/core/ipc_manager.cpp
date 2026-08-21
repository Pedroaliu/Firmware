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
namespace {

/*
 * Probe-only snapshot of the mutable endpoint-table state: the allocator bit
 * plus per-slot active/retired/generation/owner/head/count and the full queue.
 * The per-slot Spinlock is deliberately excluded — a lock object is never
 * copied, assigned, or reset by the probe.
 */
struct ProbeEndpointState {
    bool active;
    bool retired;
    uint32_t generation;
    TaskId owner;
    size_t head;
    size_t count;
    Message queue[EndpointManager::kEndpointQueueDepth];
};

struct ProbeTableState {
    bool slot_allocated[EndpointManager::kMaxEndpoints];
    ProbeEndpointState endpoints[EndpointManager::kMaxEndpoints];
};

/*
 * The full-table snapshot is ~10 KiB, too large for the boot stack. The probe
 * runs exactly once on the boot hart before jixia_release_executive_harts(),
 * so an uninitialized static needs no guard and cannot race.
 */
ProbeTableState g_probe_table_before;

} // namespace

void EndpointManager::debug_probe_capture_state() {
    EndpointManager& manager = instance();

    /* Lock order matches create/destroy: table lock -> one endpoint lock. */
    SpinlockGuard table_guard(manager.table_lock_);
    for (size_t index = 0U; index < kMaxEndpoints; ++index) {
        Endpoint& endpoint = manager.endpoints_[index];
        SpinlockGuard endpoint_guard(endpoint.lock);

        g_probe_table_before.slot_allocated[index] = manager.slot_allocated_[index];
        g_probe_table_before.endpoints[index].active = endpoint.active;
        g_probe_table_before.endpoints[index].retired = endpoint.retired;
        g_probe_table_before.endpoints[index].generation = endpoint.generation;
        g_probe_table_before.endpoints[index].owner = endpoint.owner;
        g_probe_table_before.endpoints[index].head = endpoint.head;
        g_probe_table_before.endpoints[index].count = endpoint.count;
        for (size_t entry = 0U; entry < kEndpointQueueDepth; ++entry) {
            g_probe_table_before.endpoints[index].queue[entry] = endpoint.queue[entry];
        }
    }
}

void EndpointManager::debug_probe_restore_state() {
    EndpointManager& manager = instance();

    /*
     * Lock order matches create/destroy: table lock -> one endpoint lock.
     * Only data fields are written back; the Spinlock objects are untouched.
     */
    SpinlockGuard table_guard(manager.table_lock_);
    for (size_t index = 0U; index < kMaxEndpoints; ++index) {
        Endpoint& endpoint = manager.endpoints_[index];
        SpinlockGuard endpoint_guard(endpoint.lock);
        const ProbeEndpointState& before = g_probe_table_before.endpoints[index];

        manager.slot_allocated_[index] = g_probe_table_before.slot_allocated[index];
        endpoint.active = before.active;
        endpoint.retired = before.retired;
        endpoint.generation = before.generation;
        endpoint.owner = before.owner;
        endpoint.head = before.head;
        endpoint.count = before.count;
        for (size_t entry = 0U; entry < kEndpointQueueDepth; ++entry) {
            endpoint.queue[entry] = before.queue[entry];
        }
    }
}

bool EndpointManager::debug_probe_ceiling_scenario() {
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

    return true;
}

bool EndpointManager::debug_probe_generation_ceiling() {
    /*
     * Hermeticity contract: whatever the scenario does below — and however it
     * exits, pass or fail — every slot the probe touched returns to its
     * pre-probe state before the U-mode acceptance scenario runs. The ceiling
     * and retirement evidence therefore cannot leak scratch or neighbor-slot
     * state (e.g. residual generations) into the table U mode later sees.
     */
    debug_probe_capture_state();

    const bool scenario_passed = debug_probe_ceiling_scenario();

    /* Failure paths clean up too: no half-polluted slot state remains. */
    debug_probe_restore_state();

    return scenario_passed;
}
#endif

} // namespace jixia::microkernel::ipc