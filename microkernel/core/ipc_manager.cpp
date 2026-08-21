#include "microkernel/core/ipc_manager.h"

#include "microkernel/core/hart.h"
#include "microkernel/core/scheduler.h"
#ifndef JIXIA_HOST_IPC_TORTURE
#include "microkernel/core/task.h"
#else
/*
 * The hosted torture lane never enters blocking recv/wake. It still compiles
 * the production EndpointManager translation unit, so provide only the Task
 * surface needed to type-check those fail-closed paths without importing the
 * target-only RISC-V context headers into an x86 sanitizer binary.
 */
namespace jixia::microkernel::task {

enum class TaskState : uint8_t {
    blocked_message = 'M',
};

struct Task;

struct MessageWaitNode {
    Task* previous;
    Task* next;
    bool queued;
};

struct HostedTaskContext {
    uintptr_t x[32];
};

struct Task {
    hart::HartLocal* cpu;
    HostedTaskContext context;
    TaskId tid;
    TaskState state;
    void* state_info;
    MessageWaitNode message_wait;
};

} // namespace jixia::microkernel::task
#endif
#include "microkernel/verify/trace.h"

namespace jixia::microkernel::ipc {
namespace {

[[noreturn]] void fail_closed() {
    hart::park();
}

#ifdef JIXIA_M00_08_03_02_PROBE
/* Verification-image-only witness for the C13a preemption workload. */
uint32_t g_blocked_receivers = 0U;
#endif

} // namespace

/* Caller must hold endpoint.lock. */
void EndpointManager::check_endpoint_invariant(const Endpoint& endpoint) {
    if ((endpoint.waiting_count > 0U) && (endpoint.count > 0U)) {
        /* Waiting receivers and pending messages are never both non-empty. */
        fail_closed();
    }
}

/* Caller must hold endpoint.lock. */
void EndpointManager::waiter_push(Endpoint& endpoint, task::Task& waiter) {
    waiter.message_wait.previous = endpoint.waiting_tail;
    waiter.message_wait.next = nullptr;
    if (endpoint.waiting_tail != nullptr) {
        endpoint.waiting_tail->message_wait.next = &waiter;
    } else {
        endpoint.waiting_head = &waiter;
    }
    endpoint.waiting_tail = &waiter;
    waiter.message_wait.queued = true;
    endpoint.waiting_count += 1U;
}

/* Caller must hold endpoint.lock. Unlinks and clears the membership flags. */
task::Task* EndpointManager::waiter_pop(Endpoint& endpoint) {
    task::Task* waiter = endpoint.waiting_head;
    if (waiter == nullptr) {
        return nullptr;
    }

    endpoint.waiting_head = waiter->message_wait.next;
    if (endpoint.waiting_head != nullptr) {
        endpoint.waiting_head->message_wait.previous = nullptr;
    } else {
        endpoint.waiting_tail = nullptr;
    }

    waiter->message_wait.previous = nullptr;
    waiter->message_wait.next = nullptr;
    waiter->message_wait.queued = false;
    endpoint.waiting_count -= 1U;
    return waiter;
}

/*
 * Writes a receive result into a task's saved return registers. For a woken
 * (not current) receiver this must complete before add_task publishes it
 * READY; for the immediate path the caller is the current task.
 */
void EndpointManager::write_recv_registers(task::Task& receiver, intptr_t a0,
                                           const Message& message) {
    receiver.context.x[10] = static_cast<uintptr_t>(a0);
    receiver.context.x[11] = message.words[0];
    receiver.context.x[12] = message.words[1];
    receiver.context.x[13] = message.words[2];
    receiver.context.x[14] = message.words[3];
    receiver.context.x[15] = static_cast<uintptr_t>(message.sender);
}

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
    JIXIA_VERIFY_OPERATION(operation, verify::Event::ipc_create_begin, 0U, owner, 0U, 0U);
    if (handle_out == nullptr) {
        JIXIA_VERIFY_POINT(verify::Event::ipc_create_reject, operation, 0U, owner,
                           static_cast<uint64_t>(kErrorInvalidArgument), 0U, verify::lock_none);
        return kErrorInvalidArgument;
    }

    /* Lock order: table lock -> endpoint lock (never the reverse). */
    SpinlockGuard table_guard(table_lock_);
    JIXIA_VERIFY_TEST_POINT(verify::TestPoint::ipc_create_table_locked, operation, 0U,
                            verify::lock_endpoint_table);
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
        endpoint.waiting_head = nullptr;
        endpoint.waiting_tail = nullptr;
        endpoint.waiting_count = 0U;
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

        const uint64_t handle =
            EndpointHandle{static_cast<uint32_t>(index), endpoint.generation}.raw();
        *handle_out = handle;
        JIXIA_VERIFY_POINT(verify::Event::ipc_create_publish, operation, handle, owner,
                           endpoint.generation, endpoint.count,
                           verify::lock_endpoint_table | verify::lock_endpoint);
        return 0;
    }

    JIXIA_VERIFY_POINT(verify::Event::ipc_create_reject, operation, 0U, owner,
                       static_cast<uint64_t>(kErrorNoSpace), 0U, verify::lock_endpoint_table);
    return kErrorNoSpace;
}

intptr_t EndpointManager::destroy_endpoint(TaskId caller, uint64_t handle) {
    JIXIA_VERIFY_OPERATION(operation, verify::Event::ipc_destroy_begin, handle, caller, 0U, 0U);

    const EndpointHandle parsed = EndpointHandle::from_raw(handle);
    if (!handle_well_formed(parsed)) {
        JIXIA_VERIFY_POINT(verify::Event::ipc_destroy_reject, operation, handle, caller,
                           static_cast<uint64_t>(kErrorInvalidArgument), 0U, verify::lock_none);
        return kErrorInvalidArgument;
    }

    /* Lock order: table lock -> endpoint lock (never the reverse). */
    SpinlockGuard table_guard(table_lock_);
    Endpoint& endpoint = endpoints_[parsed.index];
    SpinlockGuard endpoint_guard(endpoint.lock);
    JIXIA_VERIFY_TEST_POINT(verify::TestPoint::ipc_destroy_locked, operation, handle,
                            verify::lock_endpoint_table | verify::lock_endpoint);

    if (!slot_allocated_[parsed.index] || !endpoint.active ||
        (endpoint.generation != parsed.generation)) {
        JIXIA_VERIFY_POINT(verify::Event::ipc_destroy_reject, operation, handle, caller,
                           static_cast<uint64_t>(kErrorInvalidArgument), endpoint.generation,
                           verify::lock_endpoint_table | verify::lock_endpoint);
        return kErrorInvalidArgument;
    }

    if (endpoint.owner != caller) {
        JIXIA_VERIFY_POINT(verify::Event::ipc_destroy_reject, operation, handle, caller,
                           static_cast<uint64_t>(kErrorAccess), endpoint.owner,
                           verify::lock_endpoint_table | verify::lock_endpoint);
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
    JIXIA_VERIFY_POINT(verify::Event::ipc_destroy_dead, operation, handle, caller,
                       endpoint.generation, endpoint.retired ? 1U : 0U,
                       verify::lock_endpoint_table | verify::lock_endpoint);


    /*
     * M00-08.03.02: wake every blocked receiver exactly once with -EIDRM.
     * Each waiter is unlinked first, its return register a0 is written, and
     * only then is it published READY. add_task cannot legitimately fail on
     * an unlinked blocked task; failure is a broken kernel invariant and
     * fails closed rather than masquerading as -EAGAIN. Old-handle operations
     * after this point fail with -EINVAL through the generation check above.
     */
    while (task::Task* receiver = waiter_pop(endpoint)) {
        [[maybe_unused]] const TaskId receiver_tid = receiver->tid;
        JIXIA_VERIFY_TEST_POINT(verify::TestPoint::wake_popped, operation, handle,
                                verify::lock_endpoint_table | verify::lock_endpoint);
        receiver->state_info = nullptr;
        receiver->context.x[10] = static_cast<uintptr_t>(kErrorIdrm);
        JIXIA_VERIFY_POINT(verify::Event::ipc_destroy_wake, operation, handle, receiver_tid,
                           static_cast<uint64_t>(kErrorIdrm), endpoint.waiting_count,
                           verify::lock_endpoint_table | verify::lock_endpoint);
        JIXIA_VERIFY_POINT(verify::Event::ipc_recv_result_publish, operation, handle, receiver_tid,
                           static_cast<uint64_t>(kErrorIdrm), 0U,
                           verify::lock_endpoint_table | verify::lock_endpoint);
        JIXIA_VERIFY_TEST_POINT(verify::TestPoint::before_ready_publish, operation, handle,
                                verify::lock_endpoint_table | verify::lock_endpoint);
#ifdef JIXIA_M00_08_03_02_PROBE
        __atomic_sub_fetch(&g_blocked_receivers, 1U, __ATOMIC_RELEASE);
#endif
        if (!receiver->cpu->scheduler->add_task(*receiver)) {
            fail_closed();
        }
    }
    return 0;
}

intptr_t EndpointManager::send(TaskId sender, uint64_t handle, const uint64_t (&words)[4]) {
    JIXIA_VERIFY_OPERATION(operation, verify::Event::ipc_send_begin, handle, sender, words[0], 0U);

    const EndpointHandle parsed = EndpointHandle::from_raw(handle);
    if (!handle_well_formed(parsed)) {
        JIXIA_VERIFY_POINT(verify::Event::ipc_send_reject, operation, handle, sender,
                           static_cast<uint64_t>(kErrorInvalidArgument), 0U, verify::lock_none);
        return kErrorInvalidArgument;
    }

    /* Single endpoint lock only: no table or task-manager locks here. */
    JIXIA_VERIFY_TEST_POINT(verify::TestPoint::ipc_send_before_lock, operation, handle,
                            verify::lock_none);
    Endpoint& endpoint = endpoints_[parsed.index];
    SpinlockGuard endpoint_guard(endpoint.lock);
    JIXIA_VERIFY_TEST_POINT(verify::TestPoint::ipc_send_locked, operation, handle,
                            verify::lock_endpoint);

    if (!endpoint.active || (endpoint.generation != parsed.generation)) {
        JIXIA_VERIFY_POINT(verify::Event::ipc_send_reject, operation, handle, sender,
                           static_cast<uint64_t>(kErrorInvalidArgument), endpoint.generation,
                           verify::lock_endpoint);
        return kErrorInvalidArgument;
    }

    check_endpoint_invariant(endpoint);

    if (endpoint.waiting_count > 0U) {
        /*
         * M00-08.03.02 wakeup protocol: consume exactly one waiter, never
         * also the pending FIFO. Unlink first, then write the saved return
         * registers, then publish READY — a receiving hart may run the woken
         * task as soon as add_task succeeds, before this lock is released.
         * No handoff, no forced sender yield. add_task failure here is a
         * broken invariant and fails closed.
         */
        task::Task* receiver = waiter_pop(endpoint);
        [[maybe_unused]] const TaskId receiver_tid = receiver->tid;
        JIXIA_VERIFY_TEST_POINT(verify::TestPoint::wake_popped, operation, handle,
                                verify::lock_endpoint);
        const Message message{sender, {words[0], words[1], words[2], words[3]}};
        write_recv_registers(*receiver, 0, message);
        receiver->state_info = nullptr;
        JIXIA_VERIFY_POINT(verify::Event::ipc_send_wake, operation, handle, receiver_tid, words[0],
                           endpoint.waiting_count, verify::lock_endpoint);
        JIXIA_VERIFY_POINT(verify::Event::ipc_recv_result_publish, operation, handle, receiver_tid,
                           0U, sender, verify::lock_endpoint);
        JIXIA_VERIFY_TEST_POINT(verify::TestPoint::before_ready_publish, operation, handle,
                                verify::lock_endpoint);
#ifdef JIXIA_M00_08_03_02_PROBE
        __atomic_sub_fetch(&g_blocked_receivers, 1U, __ATOMIC_RELEASE);
#endif
        if (!receiver->cpu->scheduler->add_task(*receiver)) {
            fail_closed();
        }
        return 0;
    }

    if (endpoint.count >= kEndpointQueueDepth) {
        JIXIA_VERIFY_POINT(verify::Event::ipc_send_reject, operation, handle, sender,
                           static_cast<uint64_t>(kErrorAgain), endpoint.count,
                           verify::lock_endpoint);
        return kErrorAgain;
    }

    Message& slot = endpoint.queue[(endpoint.head + endpoint.count) % kEndpointQueueDepth];
    slot.sender = sender;
    slot.words[0] = words[0];
    slot.words[1] = words[1];
    slot.words[2] = words[2];
    slot.words[3] = words[3];
    endpoint.count += 1U;
    JIXIA_VERIFY_POINT(verify::Event::ipc_send_enqueue, operation, handle, sender, words[0],
                       endpoint.count, verify::lock_endpoint);
    return 0;
}

intptr_t EndpointManager::try_recv(uint64_t handle, Message* message_out) {
    JIXIA_VERIFY_OPERATION(operation, verify::Event::ipc_recv_begin, handle, 0U, 0U, 0U);
    const EndpointHandle parsed = EndpointHandle::from_raw(handle);
    if (!handle_well_formed(parsed) || (message_out == nullptr)) {
        JIXIA_VERIFY_POINT(verify::Event::ipc_recv_reject, operation, handle, 0U,
                           static_cast<uint64_t>(kErrorInvalidArgument), 0U, verify::lock_none);
        return kErrorInvalidArgument;
    }

    /* Single endpoint lock only: no table, task, or time locks here. */
    JIXIA_VERIFY_TEST_POINT(verify::TestPoint::ipc_recv_before_lock, operation, handle,
                            verify::lock_none);
    Endpoint& endpoint = endpoints_[parsed.index];
    SpinlockGuard endpoint_guard(endpoint.lock);
    JIXIA_VERIFY_TEST_POINT(verify::TestPoint::ipc_recv_locked, operation, handle,
                            verify::lock_endpoint);

    if (!endpoint.active || (endpoint.generation != parsed.generation)) {
        JIXIA_VERIFY_POINT(verify::Event::ipc_recv_reject, operation, handle, 0U,
                           static_cast<uint64_t>(kErrorInvalidArgument), endpoint.generation,
                           verify::lock_endpoint);
        return kErrorInvalidArgument;
    }

    if (endpoint.count == 0U) {
        JIXIA_VERIFY_POINT(verify::Event::ipc_recv_reject, operation, handle, 0U,
                           static_cast<uint64_t>(kErrorAgain), 0U, verify::lock_endpoint);
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
    JIXIA_VERIFY_POINT(verify::Event::ipc_recv_dequeue, operation, handle, message_out->sender,
                       message_out->words[0], endpoint.count, verify::lock_endpoint);
    return 0;
}

RecvResult EndpointManager::recv(task::Task& caller, uint64_t handle) {
    JIXIA_VERIFY_OPERATION(operation, verify::Event::ipc_recv_begin, handle, caller.tid, 0U, 0U);
    /*
     * A task already queued in a waiting FIFO cannot issue a syscall; reaching
     * this check with it set means kernel state corruption.
     */
    if (caller.message_wait.queued) {
        fail_closed();
    }

    const EndpointHandle parsed = EndpointHandle::from_raw(handle);
    if (!handle_well_formed(parsed)) {
        /* Rejected paths write the caller's a0 directly: it is still current. */
        caller.context.x[10] = static_cast<uintptr_t>(kErrorInvalidArgument);
        JIXIA_VERIFY_POINT(verify::Event::ipc_recv_reject, operation, handle, caller.tid,
                           static_cast<uint64_t>(kErrorInvalidArgument), 0U, verify::lock_none);
        return RecvResult{RecvStatus::rejected, {}, kErrorInvalidArgument};
    }

    /* Single endpoint lock only: no table or task-manager locks here. */
    JIXIA_VERIFY_TEST_POINT(verify::TestPoint::ipc_recv_before_lock, operation, handle,
                            verify::lock_none);
    Endpoint& endpoint = endpoints_[parsed.index];
    SpinlockGuard endpoint_guard(endpoint.lock);
    JIXIA_VERIFY_TEST_POINT(verify::TestPoint::ipc_recv_locked, operation, handle,
                            verify::lock_endpoint);

    if (!endpoint.active || (endpoint.generation != parsed.generation)) {
        caller.context.x[10] = static_cast<uintptr_t>(kErrorInvalidArgument);
        JIXIA_VERIFY_POINT(verify::Event::ipc_recv_reject, operation, handle, caller.tid,
                           static_cast<uint64_t>(kErrorInvalidArgument), endpoint.generation,
                           verify::lock_endpoint);
        return RecvResult{RecvStatus::rejected, {}, kErrorInvalidArgument};
    }

    check_endpoint_invariant(endpoint);

    if (endpoint.count > 0U) {
        /* No receiver can be waiting while a message is pending. */
        Message message = endpoint.queue[endpoint.head];
        endpoint.queue[endpoint.head] = {};
        endpoint.head = (endpoint.head + 1U) % kEndpointQueueDepth;
        endpoint.count -= 1U;
        write_recv_registers(caller, 0, message);
        JIXIA_VERIFY_POINT(verify::Event::ipc_recv_dequeue, operation, handle, message.sender,
                           message.words[0], endpoint.count, verify::lock_endpoint);
        return RecvResult{RecvStatus::delivered, message, 0};
    }

    /*
     * M00-08.03.02 atomic blocking protocol, all under endpoint.lock:
     * enqueue waiter -> blocked_message -> state_info -> set_next_runnable().
     * The SpinlockGuard releases the lock only after this hart's current_task
     * has been replaced, so a send or destroy on another hart can never see a
     * half-blocked receiver and can never wake it before the block is durable.
     */
    waiter_push(endpoint, caller);
    caller.state = task::TaskState::blocked_message;
    caller.state_info = reinterpret_cast<void*>(handle);
#ifdef JIXIA_M00_08_03_02_PROBE
    __atomic_add_fetch(&g_blocked_receivers, 1U, __ATOMIC_RELEASE);
#endif
    JIXIA_VERIFY_POINT(verify::Event::ipc_recv_wait_enqueue, operation, handle, caller.tid,
                       static_cast<uint64_t>(caller.state), endpoint.waiting_count,
                       verify::lock_endpoint);
    JIXIA_VERIFY_TEST_POINT(verify::TestPoint::wait_enqueued, operation, handle,
                            verify::lock_endpoint);
    JIXIA_VERIFY_TEST_POINT(verify::TestPoint::before_current_relinquish, operation, handle,
                            verify::lock_endpoint);
    (void)caller.cpu->scheduler->set_next_runnable();

    /*
     * The old caller may already run (and even end) on another hart once the
     * endpoint lock is released: no field of `caller` may be touched again.
     */
    return RecvResult{RecvStatus::blocked, {}, 0};
}

#ifdef JIXIA_M00_08_03_02_PROBE
size_t EndpointManager::debug_blocked_receiver_count() {
    return static_cast<size_t>(__atomic_load_n(&g_blocked_receivers, __ATOMIC_ACQUIRE));
}
#endif

#if defined(JIXIA_M00_08_03_01_PROBE) || defined(JIXIA_M00_08_03_02_PROBE)
namespace {

/*
 * Probe-only snapshot of the mutable endpoint-table state: the allocator bit
 * plus per-slot active/retired/generation/owner/head/count, the waiting-FIFO
 * links/count (M00-08.03.02), and the full queue. The per-slot Spinlock is
 * deliberately excluded — a lock object is never copied, assigned, or reset
 * by the probe.
 */
struct ProbeEndpointState {
    bool active;
    bool retired;
    uint32_t generation;
    TaskId owner;
    size_t head;
    size_t count;
    task::Task* waiting_head;
    task::Task* waiting_tail;
    size_t waiting_count;
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
        g_probe_table_before.endpoints[index].waiting_head = endpoint.waiting_head;
        g_probe_table_before.endpoints[index].waiting_tail = endpoint.waiting_tail;
        g_probe_table_before.endpoints[index].waiting_count = endpoint.waiting_count;
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
        endpoint.waiting_head = before.waiting_head;
        endpoint.waiting_tail = before.waiting_tail;
        endpoint.waiting_count = before.waiting_count;
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
