#pragma once

#include <stdint.h>

namespace jixia::microkernel::verify {

enum class Event : uint16_t {
    test_point = 1,

    ipc_create_begin,
    ipc_create_publish,
    ipc_create_reject,
    ipc_destroy_begin,
    ipc_destroy_dead,
    ipc_destroy_reject,
    ipc_send_begin,
    ipc_send_enqueue,
    ipc_send_reject,
    ipc_recv_begin,
    ipc_recv_dequeue,
    ipc_recv_reject,

    runqueue_insert_begin,
    runqueue_insert_publish,
    runqueue_insert_reject,
    runqueue_remove_begin,
    runqueue_remove_select,
    runqueue_remove_empty,
    task_current_publish,
    task_state_publish,
};

enum LockSet : uint16_t {
    lock_none = 0U,
    lock_endpoint_table = 1U << 0U,
    lock_endpoint = 1U << 1U,
    lock_task_manager = 1U << 2U,
    lock_runqueue = 1U << 3U,
    lock_delay_queue = 1U << 4U,
};

enum class TestPoint : uint16_t {
    ipc_create_table_locked = 1,
    ipc_destroy_locked,
    ipc_send_before_lock,
    ipc_send_locked,
    ipc_recv_before_lock,
    ipc_recv_locked,
    runqueue_insert_locked,
    runqueue_remove_locked,

    /* Reserved now so blocking recv uses one stable verification vocabulary. */
    wait_enqueued,
    before_current_relinquish,
    wake_popped,
    before_ready_publish,
};

#if defined(JIXIA_VERIFICATION)

void initialize();
[[nodiscard]] uint64_t begin(Event event, uint64_t object, uint64_t subject, uint64_t arg0,
                             uint64_t arg1, uint16_t lockset = lock_none);
void point(Event event, uint64_t operation, uint64_t object, uint64_t subject, uint64_t arg0,
           uint64_t arg1, uint16_t lockset);
void test_point(TestPoint test_point, uint64_t operation, uint64_t object, uint16_t lockset);
void dump();

#endif

} // namespace jixia::microkernel::verify

/*
 * Macro boundary is intentional: in a production build the arguments are not
 * evaluated and no empty function call survives an -O0 build.  Verification
 * code and data therefore have exactly zero production footprint.
 */
#if defined(JIXIA_VERIFICATION)
#define JIXIA_VERIFY_INITIALIZE() (::jixia::microkernel::verify::initialize())
#define JIXIA_VERIFY_OPERATION(name, event, object, subject, arg0, arg1)                           \
    const uint64_t name =                                                                          \
        ::jixia::microkernel::verify::begin((event), (object), (subject), (arg0), (arg1))
#define JIXIA_VERIFY_LOCKED_OPERATION(name, event, object, subject, arg0, arg1, lockset)           \
    const uint64_t name = ::jixia::microkernel::verify::begin(                                    \
        (event), (object), (subject), (arg0), (arg1), (lockset))
#define JIXIA_VERIFY_BEGIN(event, object, subject, arg0, arg1)                                    \
    (::jixia::microkernel::verify::begin((event), (object), (subject), (arg0), (arg1)))
#define JIXIA_VERIFY_POINT(event, operation, object, subject, arg0, arg1, lockset)                 \
    (::jixia::microkernel::verify::point((event), (operation), (object), (subject), (arg0),        \
                                         (arg1), (lockset)))
#define JIXIA_VERIFY_TEST_POINT(selected_point, operation, object, lockset)                        \
    (::jixia::microkernel::verify::test_point((selected_point), (operation), (object), (lockset)))
#define JIXIA_VERIFY_DUMP() (::jixia::microkernel::verify::dump())
#else
#define JIXIA_VERIFY_INITIALIZE() ((void)0)
#define JIXIA_VERIFY_OPERATION(name, event, object, subject, arg0, arg1) ((void)0)
#define JIXIA_VERIFY_LOCKED_OPERATION(name, event, object, subject, arg0, arg1, lockset) ((void)0)
#define JIXIA_VERIFY_BEGIN(event, object, subject, arg0, arg1) (0ULL)
#define JIXIA_VERIFY_POINT(event, operation, object, subject, arg0, arg1, lockset) ((void)0)
#define JIXIA_VERIFY_TEST_POINT(selected_point, operation, object, lockset) ((void)0)
#define JIXIA_VERIFY_DUMP() ((void)0)
#endif
