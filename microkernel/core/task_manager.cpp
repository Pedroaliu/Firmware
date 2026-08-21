#include "microkernel/core/task_manager.h"

#include "microkernel/core/hart.h"
#include "microkernel/core/scheduler.h"
#include "microkernel/core/vmm_manager.h"
#include "microkernel/verify/trace.h"

extern "C" char jixia_user_idle_task[];
extern "C" char jixia_user_task_end_stub[];

namespace {

constexpr size_t kTaskStackSize = 4096U;

alignas(kTaskStackSize) uint8_t
    g_task_stacks[jixia::microkernel::task::TaskManager::kMaxTasks][kTaskStackSize];

void clear_bytes(void* address, size_t size) {
    auto* bytes = static_cast<volatile uint8_t*>(address);
    for (size_t index = 0U; index < size; ++index) {
        bytes[index] = 0U;
    }
}

[[noreturn]] void fail_closed() {
    jixia::microkernel::hart::park();
}

} // namespace

namespace jixia::microkernel::task {

TaskManager::TaskManager()
    : tasks_{}, trackers_{}, task_slots_used_{}, tracker_slots_used_{}, root_tasks_(nullptr),
      next_tid_(1U), initial_task_id_(0U), initialized_(false) {
}

TaskManager& TaskManager::instance() {
    return Singleton<TaskManager>::instance();
}

bool TaskManager::initialize() {
    SpinlockGuard guard(lock_);

    if (initialized_) {
        return true;
    }

    for (size_t index = 0U; index < kMaxTasks; ++index) {
        tasks_[index] = {};
        trackers_[index] = {};
        task_slots_used_[index] = false;
        tracker_slots_used_[index] = false;
    }

    clear_bytes(g_task_stacks, sizeof(g_task_stacks));

    /*
     * The bootstrap VSpace is shared by all harts. Install every bounded
     * task-stack mapping before secondary harts are released so later task
     * creation never mutates a live shared page table without TLB shootdown.
     */
    for (size_t index = 0U; index < kMaxTasks; ++index) {
        const uintptr_t stack_bottom = reinterpret_cast<uintptr_t>(&g_task_stacks[index][0]);
        if (!memory::VmmManager::instance().map_boot_task_stack(stack_bottom, kTaskStackSize)) {
            return false;
        }
    }

    root_tasks_ = nullptr;
    next_tid_ = 1U;
    initial_task_id_ = 0U;
    initialized_ = true;
    return true;
}

Task* TaskManager::create_idle_task(hart::HartLocal& owner) {
    SpinlockGuard guard(lock_);
    const auto& address_space = memory::VmmManager::instance().boot_address_space();
    return create_task_locked(reinterpret_cast<EntryPoint>(jixia_user_idle_task), 0U, address_space,
                              owner, true, true, false);
}

Task* TaskManager::create_task(EntryPoint entry, uintptr_t argument,
                               const jixia::arch::riscv::sv39::AddressSpace& address_space,
                               hart::HartLocal& owner, bool kernel_parent) {
    SpinlockGuard guard(lock_);
    return create_task_locked(entry, argument, address_space, owner, kernel_parent, false, true);
}

Task* TaskManager::create_task_locked(EntryPoint entry, uintptr_t argument,
                                      const jixia::arch::riscv::sv39::AddressSpace& address_space,
                                      hart::HartLocal& owner, bool kernel_parent, bool idle,
                                      bool with_stack) {
    if (!initialized_ || !address_space.valid() ||
        !memory::VmmManager::instance().is_boot_user_entry(entry)) {
        return nullptr;
    }

    size_t slot_index = 0U;
    Task* task = allocate_task_locked(&slot_index);
    TaskTracker* tracker = allocate_tracker_locked();
    if (task == nullptr || tracker == nullptr) {
        if (task != nullptr) {
            release_task_locked(*task);
        }
        if (tracker != nullptr) {
            release_tracker_locked(*tracker);
        }
        return nullptr;
    }

    uintptr_t stack_bottom = 0U;
    uintptr_t stack_top = 0U;
    if (with_stack) {
        stack_bottom = reinterpret_cast<uintptr_t>(&g_task_stacks[slot_index][0]);
        stack_top = stack_bottom + kTaskStackSize;
        clear_bytes(reinterpret_cast<void*>(stack_bottom), kTaskStackSize);

        if (!memory::VmmManager::instance().map_boot_task_stack(stack_bottom, kTaskStackSize)) {
            release_tracker_locked(*tracker);
            release_task_locked(*task);
            return nullptr;
        }
    }

    task->cpu = &owner;
    task->address_space = address_space;
    task->tid = next_tid_++;
    task->affinity_pinned = 0U;
    task->state = TaskState::ready;
    task->state_info = nullptr;
    task->tracker = tracker;
    task->stack_bottom = stack_bottom;
    task->stack_top = stack_top;
    task->detached = false;
    task->idle = idle;
    task->queued = false;
    task->previous = nullptr;
    task->next = nullptr;
    task->message_wait = {};

    initialize_user(task->context, entry, argument, stack_top,
                    reinterpret_cast<uintptr_t>(jixia_user_task_end_stub),
                    jixia::arch::riscv::sv39::satp_value(address_space));

    tracker->key = task->tid;
    tracker->task = task;
    tracker->status = ExitStatus::running;
    tracker->return_value = 0U;
    tracker->wait = {};
    tracker->entry_point = entry;

    Task* parent_task = kernel_parent ? nullptr : get_current_task();
    link_tracker_locked(*tracker, parent_task == nullptr ? nullptr : parent_task->tracker);
    return task;
}

Task* TaskManager::allocate_task_locked(size_t* slot_index) {
    for (size_t index = 0U; index < kMaxTasks; ++index) {
        if (!task_slots_used_[index]) {
            task_slots_used_[index] = true;
            tasks_[index] = {};
            *slot_index = index;
            return &tasks_[index];
        }
    }

    return nullptr;
}

TaskTracker* TaskManager::allocate_tracker_locked() {
    for (size_t index = 0U; index < kMaxTasks; ++index) {
        if (!tracker_slots_used_[index]) {
            tracker_slots_used_[index] = true;
            trackers_[index] = {};
            return &trackers_[index];
        }
    }

    return nullptr;
}

void TaskManager::release_task_locked(Task& task) {
    const size_t index = static_cast<size_t>(&task - &tasks_[0]);
    if (index >= kMaxTasks) {
        fail_closed();
    }

    /*
     * M00-08.03.02: a task still queued in an endpoint waiting FIFO must never be
     * freed — the FIFO holds raw Task pointers, so releasing the slot here would
     * be a silent use-after-free. Full task-exit IPC cleanup is a later
     * increment; refusing (parking) is the fail-closed behavior for now.
     */
    if (task.queued || task.delay.queued || task.message_wait.queued) {
        fail_closed();
    }

    if (task.stack_bottom != 0U) {
        clear_bytes(reinterpret_cast<void*>(task.stack_bottom), kTaskStackSize);
    }

    tasks_[index] = {};
    task_slots_used_[index] = false;
}

void TaskManager::release_tracker_locked(TaskTracker& tracker) {
    const size_t index = static_cast<size_t>(&tracker - &trackers_[0]);
    if (index >= kMaxTasks) {
        fail_closed();
    }

    trackers_[index] = {};
    tracker_slots_used_[index] = false;
}

void TaskManager::link_tracker_locked(TaskTracker& tracker, TaskTracker* parent) {
    tracker.parent = parent;
    tracker.previous_sibling = nullptr;

    TaskTracker*& head = parent == nullptr ? root_tasks_ : parent->first_child;
    tracker.next_sibling = head;
    if (head != nullptr) {
        head->previous_sibling = &tracker;
    }
    head = &tracker;
}

void TaskManager::unlink_tracker_locked(TaskTracker& tracker) {
    TaskTracker*& head = tracker.parent == nullptr ? root_tasks_ : tracker.parent->first_child;

    if (tracker.previous_sibling != nullptr) {
        tracker.previous_sibling->next_sibling = tracker.next_sibling;
    } else {
        head = tracker.next_sibling;
    }

    if (tracker.next_sibling != nullptr) {
        tracker.next_sibling->previous_sibling = tracker.previous_sibling;
    }

    tracker.previous_sibling = nullptr;
    tracker.next_sibling = nullptr;
}

void TaskManager::remove_tracker_locked(TaskTracker& tracker) {
    TaskTracker* new_parent = tracker.parent;
    unlink_tracker_locked(tracker);

    while (tracker.first_child != nullptr) {
        TaskTracker* child = tracker.first_child;
        unlink_tracker_locked(*child);
        link_tracker_locked(*child, new_parent);

        /*
         * Match Hostboot's orphan policy: a completed child that becomes
         * kernel-parented has nobody left who can join it. Reap a clean
         * child now; retain no crashed orphan as if it were harmless.
         */
        if (new_parent == nullptr && child->task == nullptr) {
            if (child->status == ExitStatus::crashed) {
                fail_closed();
            }
            remove_tracker_locked(*child);
        }
    }

    release_tracker_locked(tracker);
}

TaskTracker* TaskManager::find_child_locked(const TaskTracker& parent, int64_t tid) const {
    if (tid >= 0) {
        TaskTracker* child = parent.first_child;
        while (child != nullptr) {
            if (child->key == static_cast<TaskId>(tid)) {
                return child;
            }
            child = child->next_sibling;
        }
        return nullptr;
    }

    /* wait(-1) first consumes any completed child, independent of order. */
    TaskTracker* child = parent.first_child;
    while (child != nullptr) {
        if (child->task == nullptr) {
            return child;
        }
        child = child->next_sibling;
    }

    /* A running child means the caller must block; null means no children. */
    return parent.first_child;
}

void TaskManager::end_task(Task& task, uintptr_t return_value, ExitStatus status) {
    const bool ending_current = get_current_task() == &task;
    if (!ending_current && (task.queued || task.state == TaskState::running)) {
        fail_closed();
    }

    task.state = TaskState::ended;

    if (ending_current) {
        (void)task.cpu->scheduler->set_next_runnable();
    }

    SpinlockGuard guard(lock_);
    TaskTracker* tracker = task.tracker;
    tracker->status = status;
    tracker->return_value = return_value;
    tracker->task = nullptr;

    if (task.detached) {
        if (status == ExitStatus::crashed) {
            fail_closed();
        }
        remove_tracker_locked(*tracker);
    } else if (tracker->parent != nullptr && tracker->parent->wait.active) {
        TaskTracker* parent = tracker->parent;
        if (parent->wait.tid < 0 || parent->wait.tid == static_cast<int64_t>(task.tid)) {
            if (parent->task == nullptr) {
                fail_closed();
            }

            parent->wait.active = false;
            parent->task->context.x[10] = task.tid;
            remove_tracker_locked(*tracker);
            __atomic_thread_fence(__ATOMIC_RELEASE);
            if (!parent->task->cpu->scheduler->add_task(*parent->task)) {
                fail_closed();
            }
        }
    } else if (tracker->parent == nullptr) {
        if (status == ExitStatus::crashed) {
            fail_closed();
        }
        remove_tracker_locked(*tracker);
    }

    release_task_locked(task);
}

WaitResult TaskManager::wait_task(Task& task, int64_t tid) {
    if (get_current_task() != &task) {
        fail_closed();
    }

    SpinlockGuard guard(lock_);
    TaskTracker* child = find_child_locked(*task.tracker, tid);

    if (child == nullptr) {
        return WaitResult::no_child;
    }

    if (child->task == nullptr) {
        task.context.x[10] = child->key;
        remove_tracker_locked(*child);
        return WaitResult::completed;
    }

    task.tracker->wait = {
        .active = true,
        .tid = tid,
        .status_address = 0U,
        .return_value_address = 0U,
    };
    task.state = TaskState::blocked_join;
    task.state_info = reinterpret_cast<void*>(static_cast<uintptr_t>(tid));
    (void)task.cpu->scheduler->set_next_runnable();
    return WaitResult::blocked;
}

bool TaskManager::detach_task(Task& task) {
    if (get_current_task() != &task) {
        fail_closed();
    }

    SpinlockGuard guard(lock_);
    if (task.state == TaskState::ended) {
        return false;
    }

    task.detached = true;
    return true;
}

Task* TaskManager::get_current_task() {
    return hart::current().current_task;
}

void TaskManager::set_current_task(Task& task) {
    /* message_wait.queued: a blocked receiver is in no run queue (M00-08.03.02). */
    if (task.state == TaskState::ended || task.queued || task.delay.queued ||
        task.message_wait.queued) {
        fail_closed();
    }

    hart::HartLocal& local = hart::current();
    task.cpu = &local;
    task.state = TaskState::running;
    local.current_task = &task;
#if defined(JIXIA_VERIFICATION)
    /* Idle's yield loop is intentionally not a trace producer. */
    if (!task.idle) {
        JIXIA_VERIFY_POINT(verify::Event::task_current_publish, 0U, local.index, task.tid,
                           static_cast<uint64_t>(task.state), 0U, verify::lock_none);
    }
#endif
}

void TaskManager::save_current_context(const jixia::arch::riscv::TrapFrame& frame) {
    Task* current = get_current_task();
    if (current == nullptr) {
        fail_closed();
    }

    save_from_trap(current->context, frame);
}

void TaskManager::restore_current_context(jixia::arch::riscv::TrapFrame& frame) {
    Task* current = get_current_task();
    if (current == nullptr) {
        fail_closed();
    }

    restore_to_trap(current->context, frame);
    __asm__ volatile("csrw satp, %0\nsfence.vma zero, zero"
                     :
                     : "r"(current->context.satp)
                     : "memory");
}

void TaskManager::set_initial_task(Task& task) {
    initial_task_id_ = task.tid;
}

TaskId TaskManager::initial_task_id() const {
    return initial_task_id_;
}

} // namespace jixia::microkernel::task
