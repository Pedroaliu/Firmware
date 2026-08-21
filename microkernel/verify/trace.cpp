#include "microkernel/verify/trace.h"

#if defined(JIXIA_VERIFICATION)

#include <stddef.h>

#include "microkernel/console/printk.h"
#include "microkernel/core/hart.h"
#include "platform/qemu_virt/timer.h"

#ifndef JIXIA_VERIFICATION_SEED
#define JIXIA_VERIFICATION_SEED 1ULL
#endif

#ifndef JIXIA_VERIFICATION_TRACE_RECORDS
#define JIXIA_VERIFICATION_TRACE_RECORDS 1024U
#endif

namespace jixia::microkernel::verify {
namespace {

struct TraceRecord {
    uint64_t sequence;
    uint64_t timestamp;
    uint64_t operation;
    uint64_t object;
    uint64_t subject;
    uint64_t arg0;
    uint64_t arg1;
    uint32_t hart;
    uint16_t event;
    uint16_t lockset;
};

static_assert(sizeof(TraceRecord) == 64U);

struct alignas(64) TraceBuffer {
    uint64_t head;
    uint64_t dropped;
    TraceRecord records[JIXIA_VERIFICATION_TRACE_RECORDS];
};

TraceBuffer g_trace_buffers[hart::kMaxHarts];
uint64_t g_global_sequence = 0U;

const char* event_name(Event event) {
    switch (event) {
    case Event::test_point:
        return "test_point";
    case Event::ipc_create_begin:
        return "ipc_create_begin";
    case Event::ipc_create_publish:
        return "ipc_create_publish";
    case Event::ipc_create_reject:
        return "ipc_create_reject";
    case Event::ipc_destroy_begin:
        return "ipc_destroy_begin";
    case Event::ipc_destroy_dead:
        return "ipc_destroy_dead";
    case Event::ipc_destroy_wake:
        return "ipc_destroy_wake";
    case Event::ipc_destroy_reject:
        return "ipc_destroy_reject";
    case Event::ipc_send_begin:
        return "ipc_send_begin";
    case Event::ipc_send_enqueue:
        return "ipc_send_enqueue";
    case Event::ipc_send_wake:
        return "ipc_send_wake";
    case Event::ipc_send_reject:
        return "ipc_send_reject";
    case Event::ipc_recv_begin:
        return "ipc_recv_begin";
    case Event::ipc_recv_dequeue:
        return "ipc_recv_dequeue";
    case Event::ipc_recv_wait_enqueue:
        return "ipc_recv_wait_enqueue";
    case Event::ipc_recv_result_publish:
        return "ipc_recv_result_publish";
    case Event::ipc_recv_reject:
        return "ipc_recv_reject";
    case Event::runqueue_insert_begin:
        return "runqueue_insert_begin";
    case Event::runqueue_insert_publish:
        return "runqueue_insert_publish";
    case Event::runqueue_insert_reject:
        return "runqueue_insert_reject";
    case Event::runqueue_remove_begin:
        return "runqueue_remove_begin";
    case Event::runqueue_remove_select:
        return "runqueue_remove_select";
    case Event::runqueue_remove_empty:
        return "runqueue_remove_empty";
    case Event::task_current_publish:
        return "task_current_publish";
    case Event::task_state_publish:
        return "task_state_publish";
    }

    return "unknown";
}

uint64_t record(Event event, uint64_t operation, uint64_t object, uint64_t subject, uint64_t arg0,
                uint64_t arg1, uint16_t lockset) {
    const hart::HartLocal& local = hart::current();
    if (local.index >= hart::kMaxHarts) {
        return 0U;
    }

    TraceBuffer& buffer = g_trace_buffers[local.index];
    const uint64_t index = __atomic_load_n(&buffer.head, __ATOMIC_RELAXED);
    if (index >= JIXIA_VERIFICATION_TRACE_RECORDS) {
        __atomic_fetch_add(&buffer.dropped, 1U, __ATOMIC_RELAXED);
        return 0U;
    }

    const uint64_t sequence = __atomic_fetch_add(&g_global_sequence, 1U, __ATOMIC_RELAXED) + 1U;
    TraceRecord& destination = buffer.records[index];
    destination.sequence = sequence;
    destination.timestamp = jixia::platform::qemu_virt::timer::read_time();
    destination.operation = operation == 0U ? sequence : operation;
    destination.object = object;
    destination.subject = subject;
    destination.arg0 = arg0;
    destination.arg1 = arg1;
    destination.hart = local.index;
    destination.event = static_cast<uint16_t>(event);
    destination.lockset = lockset;

    /* Publish a complete record to the boot-hart dumper. */
    __atomic_store_n(&buffer.head, index + 1U, __ATOMIC_RELEASE);
    return sequence;
}

} // namespace

void initialize() {
    g_global_sequence = 0U;
    for (TraceBuffer& buffer : g_trace_buffers) {
        buffer.head = 0U;
        buffer.dropped = 0U;
        for (TraceRecord& record_entry : buffer.records) {
            record_entry = {};
        }
    }
}

uint64_t begin(Event event, uint64_t object, uint64_t subject, uint64_t arg0, uint64_t arg1,
               uint16_t lockset) {
    return record(event, 0U, object, subject, arg0, arg1, lockset);
}

void point(Event event, uint64_t operation, uint64_t object, uint64_t subject, uint64_t arg0,
           uint64_t arg1, uint16_t lockset) {
    (void)record(event, operation, object, subject, arg0, arg1, lockset);
}

void test_point(TestPoint selected, uint64_t operation, uint64_t object, uint16_t lockset) {
    (void)record(Event::test_point, operation, object, 0U, static_cast<uint64_t>(selected),
                 JIXIA_VERIFICATION_SEED, lockset);

#if defined(JIXIA_VERIFICATION_JITTER)
    uint64_t random = JIXIA_VERIFICATION_SEED ^ operation ^ object ^
                      (static_cast<uint64_t>(selected) << 32U) ^ hart::current().index;
    random ^= random << 13U;
    random ^= random >> 7U;
    random ^= random << 17U;
    const uint32_t spins = static_cast<uint32_t>(random & 0x3FFU);
    for (uint32_t index = 0U; index < spins; ++index) {
        __asm__ volatile("nop" ::: "memory");
    }
#endif
}

void dump() {
    printk("JIXIA_VERIFY_TRACE_BEGIN: seed=%lu records_per_hart=%lu\n",
           static_cast<unsigned long>(JIXIA_VERIFICATION_SEED),
           static_cast<unsigned long>(JIXIA_VERIFICATION_TRACE_RECORDS));

    for (hart::HartIndex hart_index = 0U; hart_index < hart::kMaxHarts; ++hart_index) {
        const TraceBuffer& buffer = g_trace_buffers[hart_index];
        const uint64_t head = __atomic_load_n(&buffer.head, __ATOMIC_ACQUIRE);
        for (uint64_t index = 0U; index < head; ++index) {
            const TraceRecord& entry = buffer.records[index];
            printk(
                "JIXIA_VERIFY_TRACE: seq=%lu time=%lu hart=%lu op=%lu event=%s "
                "lockset=%lu object=%lu subject=%lu arg0=%lu arg1=%lu\n",
                static_cast<unsigned long>(entry.sequence),
                static_cast<unsigned long>(entry.timestamp), static_cast<unsigned long>(entry.hart),
                static_cast<unsigned long>(entry.operation),
                event_name(static_cast<Event>(entry.event)),
                static_cast<unsigned long>(entry.lockset), static_cast<unsigned long>(entry.object),
                static_cast<unsigned long>(entry.subject), static_cast<unsigned long>(entry.arg0),
                static_cast<unsigned long>(entry.arg1));
        }
        printk("JIXIA_VERIFY_TRACE_HART: hart=%lu records=%lu dropped=%lu\n",
               static_cast<unsigned long>(hart_index), static_cast<unsigned long>(head),
               static_cast<unsigned long>(__atomic_load_n(&buffer.dropped, __ATOMIC_RELAXED)));
    }

    printk("JIXIA_VERIFY_TRACE_END: sequence=%lu\n",
           static_cast<unsigned long>(__atomic_load_n(&g_global_sequence, __ATOMIC_RELAXED)));
}

} // namespace jixia::microkernel::verify

#endif
