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

/* A message is a pure value: sender identity plus four payload words. */
struct Message {
    TaskId sender;
    uint64_t words[4];
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
 * M00-08.03.01 non-blocking Endpoint/Message IPC.
 *
 * Static endpoint table (no dynamic allocation), one spinlock per endpoint,
 * fixed-depth FIFO per endpoint. Lock rules (accepted ABI doc):
 *   - send/try_recv take only the target endpoint lock
 *   - create/destroy take table lock -> endpoint lock (never reversed)
 *   - never two endpoint locks at once
 *   - no TaskManager/TimeManager lock inside an endpoint lock
 * No operation blocks, schedules, or wakes a task.
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

    /* Owner-only teardown: -EACCES for non-owners, -EINVAL for stale handles. */
    [[nodiscard]] intptr_t destroy_endpoint(TaskId caller, uint64_t handle);

    /* Enqueues sender + payload. -EAGAIN when the FIFO is full. Never blocks. */
    [[nodiscard]] intptr_t send(TaskId sender, uint64_t handle, const uint64_t (&words)[4]);

    /* Pops in FIFO order into *message_out. -EAGAIN when empty. Never blocks. */
    [[nodiscard]] intptr_t try_recv(uint64_t handle, Message* message_out);

#ifdef JIXIA_M00_08_03_01_PROBE
    /*
     * Probe-only white-box acceptance for the generation ceiling: drives one
     * scratch slot to kMaxGeneration, destroys it, and requires permanent
     * retirement (create must skip the slot; the epoch never wraps to 1; no
     * published handle has bit 63 set). Restores pristine slot state so the
     * U-mode scenario still sees a full 16-slot table.
     */
    [[nodiscard]] static bool debug_probe_generation_ceiling();
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
    };

    /* Index in range AND generation in [1, kMaxGeneration] (bit 63 clear). */
    [[nodiscard]] static bool handle_well_formed(const EndpointHandle& handle);

    static void clear_queue(Endpoint& endpoint);

    /* Guards only slot allocation; endpoint fields are guarded by endpoint.lock. */
    Spinlock table_lock_;
    bool slot_allocated_[kMaxEndpoints];
    Endpoint endpoints_[kMaxEndpoints];
};

} // namespace jixia::microkernel::ipc