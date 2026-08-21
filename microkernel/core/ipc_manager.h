#pragma once

#include <stddef.h>
#include <stdint.h>

#include "microkernel/core/singleton.h"
#include "microkernel/core/spinlock.h"
#include "microkernel/core/task.h"

namespace jixia::microkernel::ipc {

using jixia::microkernel::task::TaskId;

/*
 * errno values returned by the IPC layer (asm-generic numbering, negated).
 * -EAGAIN doubles as -EWOULDBLOCK (value 11) for the non-blocking operations.
 */
inline constexpr intptr_t kErrorInvalidArgument = -22;
inline constexpr intptr_t kErrorAgain = -11;
inline constexpr intptr_t kErrorAccess = -13;
inline constexpr intptr_t kErrorNoSpace = -28;
inline constexpr intptr_t kErrorIdrm = -43;

/* A message is a pure value: sender identity plus four payload words. */
struct Message {
    TaskId sender;
    uint64_t words[4];
};

/*
 * M00-08.03.02 blocking recv outcome. `blocked` means the caller has been
 * queued in the endpoint waiting FIFO and this hart's current_task was already
 * switched away inside recv(): the caller must not be touched again by the
 * syscall handler.
 */
enum class RecvStatus : uint8_t {
    delivered, /* a pending message was popped into the caller's return registers */
    blocked,   /* caller is in the waiting FIFO; CPU already switched to a new task */
    rejected,  /* -EINVAL written into the caller's a0; the caller is still running */
};

struct RecvResult {
    RecvStatus status;
    Message message; /* valid only when status == delivered */
    intptr_t error;  /* valid only when status == rejected */
};

/*
 * Typed endpoint identity handed to U-mode as one uint64_t.
 * Layout (little-endian RV64): low 32 bits = table index, high 32 bits =
 * generation epoch. Generation 0 never appears in a valid handle, and the
 * epoch is capped at kMaxGeneration so bit 63 of a legal handle is always 0.
 */
struct EndpointHandle {
    uint32_t index;
    uint32_t generation;

    [[nodiscard]] constexpr uint64_t raw() const {
        return (static_cast<uint64_t>(generation) << 32U) | static_cast<uint64_t>(index);
    }

    [[nodiscard]] static constexpr EndpointHandle from_raw(uint64_t value) {
        return EndpointHandle{
            static_cast<uint32_t>(value),
            static_cast<uint32_t>(value >> 32U),
        };
    }
};

/*
 * M00-08.03.01 non-blocking + M00-08.03.02 blocking Endpoint/Message IPC.
 *
 * Static endpoint table (no dynamic allocation), one spinlock per endpoint,
 * fixed-depth FIFO per endpoint, plus (since .03.02) a FIFO of blocked
 * receivers per endpoint. Lock rules (frozen ABI docs):
 *   - send/try_recv/recv take only the target endpoint lock
 *   - create/destroy take table lock -> endpoint lock (never reversed)
 *   - never two endpoint locks at once
 *   - inside an endpoint lock the IPC layer may reach the scheduler
 *     (add_task / set_next_runnable, i.e. runqueue + delay-queue locks) in
 *     one direction only; no TaskManager lock is ever taken inside an
 *     endpoint lock
 * try_recv never blocks or schedules. recv blocks exactly as specified by the
 * M00-08.03.02 atomic blocking protocol; send/destroy wake waiters under the
 * endpoint lock.
 *
 * The singleton follows the M00-08 boot-hart-first rule: Kernel::ipc_bootstrap
 * constructs it on the boot hart before secondary harts are released (hosted
 * thread-safe-static support is disabled in this firmware).
 */
class EndpointManager final {
  public:
    static constexpr size_t kMaxEndpoints = 16U;
    static constexpr size_t kEndpointQueueDepth = 16U;

    /*
     * Generation ceiling: keeps bit 63 of every published handle 0 and bounds
     * the epoch space. A slot whose destroy would pass the ceiling retires
     * permanently instead of wrapping, so an epoch can never repeat (no ABA
     * aliasing of stale handles).
     */
    static constexpr uint32_t kMaxGeneration = 0x7FFFFFFFU;

    static EndpointManager& instance();

    /* Allocates a slot; writes the typed handle. -ENOSPC when the table is full. */
    [[nodiscard]] intptr_t create_endpoint(TaskId owner, uint64_t* handle_out);

    /*
     * Owner-only teardown: -EACCES for non-owners, -EINVAL for stale handles.
     * Every blocked receiver is woken with -EIDRM; when woken_receivers is
     * non-null it receives the number of tasks woken (M00-08.03.02).
     */
    [[nodiscard]] intptr_t destroy_endpoint(TaskId caller, uint64_t handle,
                                            size_t* woken_receivers = nullptr);

    /*
     * Enqueues sender + payload. -EAGAIN when the FIFO is full. If a receiver
     * is blocked on the endpoint, exactly one is woken instead (M00-08.03.02);
     * when woken_receiver is non-null it receives that task's tid. Never blocks.
     */
    [[nodiscard]] intptr_t send(TaskId sender, uint64_t handle, const uint64_t (&words)[4],
                                TaskId* woken_receiver = nullptr);

    /*
     * M00-08.03.02 blocking receive. The caller must be the current task of the
     * calling hart. On an empty endpoint the caller is atomically enqueued in
     * the endpoint waiting FIFO, marked blocked_message, and this hart's
     * current_task is switched (all under the endpoint lock; the lock is
     * released only after the switch). After a `blocked` result the caller
     * Task must not be touched again by the caller of this method.
     */
    [[nodiscard]] RecvResult recv(task::Task& caller, uint64_t handle);

    /* Pops in FIFO order into *message_out. -EAGAIN when empty. Never blocks. */
    [[nodiscard]] intptr_t try_recv(uint64_t handle, Message* message_out);

#if defined(JIXIA_M00_08_03_01_PROBE) || defined(JIXIA_M00_08_03_02_PROBE)
    /*
     * Probe-only white-box acceptance for the generation ceiling: drives one
     * scratch slot to kMaxGeneration, destroys it, and requires permanent
     * retirement (create must skip the slot; the epoch never wraps to 1; no
     * published handle has bit 63 set). The probe is hermetic: on every exit
     * path, pass or fail, every slot it touched returns to its pre-probe
     * state — active, retired, generation, owner, head, count, waiting FIFO,
     * queue, and slot_allocated_ (Spinlock objects are never reset or
     * assigned) — so the U-mode scenario still sees a full 16-slot table.
     */
    [[nodiscard]] static bool debug_probe_generation_ceiling();
#endif

#ifdef JIXIA_M00_08_03_02_PROBE
    /* Probe-only: exact count of tasks currently queued in waiting FIFOs. */
    [[nodiscard]] static size_t debug_blocked_receiver_count();
#endif

  private:
    friend class jixia::microkernel::Singleton<EndpointManager>;

    EndpointManager();

    struct Endpoint {
        Spinlock lock;
        bool active;
        bool retired; /* ceiling reached: never reallocated, epoch frozen */
        uint32_t generation;
        TaskId owner;
        size_t head;
        size_t count;
        Message queue[kEndpointQueueDepth];
        /* M00-08.03.02: FIFO of tasks blocked in recv on this endpoint. */
        task::Task* waiting_head;
        task::Task* waiting_tail;
        size_t waiting_count;
    };

    /* Index in range AND generation in [1, kMaxGeneration] (bit 63 clear). */
    [[nodiscard]] static bool handle_well_formed(const EndpointHandle& handle);

    static void clear_queue(Endpoint& endpoint);

    /* M00-08.03.02 waiting-FIFO helpers; callers hold endpoint.lock. */
    static void check_endpoint_invariant(const Endpoint& endpoint);
    static void waiter_push(Endpoint& endpoint, task::Task& waiter);
    [[nodiscard]] static task::Task* waiter_pop(Endpoint& endpoint);
    static void write_recv_registers(task::Task& receiver, intptr_t a0, const Message& message);

#if defined(JIXIA_M00_08_03_01_PROBE) || defined(JIXIA_M00_08_03_02_PROBE)
    /* Probe-only: full-table snapshot, restore, and scenario body (see .cpp). */
    static void debug_probe_capture_state();
    static void debug_probe_restore_state();
    [[nodiscard]] static bool debug_probe_ceiling_scenario();
#endif

    /* Guards only slot allocation; endpoint fields are guarded by endpoint.lock. */
    Spinlock table_lock_;
    bool slot_allocated_[kMaxEndpoints];
    Endpoint endpoints_[kMaxEndpoints];
};

} // namespace jixia::microkernel::ipc