#pragma once

#include <stddef.h>
#include <stdint.h>

#include "microkernel/core/singleton.h"
#include "microkernel/core/spinlock.h"
#include "microkernel/core/task.h"

namespace jixia::microkernel::hart {
struct HartLocal;
}

namespace jixia::microkernel::task {

enum class WaitResult : uint8_t {
    completed,
    blocked,
    no_child,
};

class TaskManager final {
  public:
    static constexpr size_t kMaxTasks = 16U;

    static TaskManager& instance();

    void initialize();

    [[nodiscard]] Task* create_idle_task(hart::HartLocal& owner);
    [[nodiscard]] Task* create_task(EntryPoint entry, uintptr_t argument,
                                    const jixia::arch::riscv::sv39::AddressSpace& address_space,
                                    hart::HartLocal& owner, bool kernel_parent = false);

    void end_task(Task& task, uintptr_t return_value, ExitStatus status);
    [[nodiscard]] WaitResult wait_task(Task& task, int64_t tid);
    [[nodiscard]] bool detach_task(Task& task);

    [[nodiscard]] static Task* get_current_task();
    static void set_current_task(Task& task);

    static void save_current_context(const jixia::arch::riscv::TrapFrame& frame);
    static void restore_current_context(jixia::arch::riscv::TrapFrame& frame);

    void set_initial_task(Task& task);
    [[nodiscard]] TaskId initial_task_id() const;

  private:
    friend class jixia::microkernel::Singleton<TaskManager>;

    TaskManager();

    [[nodiscard]] Task*
    create_task_locked(EntryPoint entry, uintptr_t argument,
                       const jixia::arch::riscv::sv39::AddressSpace& address_space,
                       hart::HartLocal& owner, bool kernel_parent, bool idle, bool with_stack);

    [[nodiscard]] Task* allocate_task_locked(size_t* slot_index);
    [[nodiscard]] TaskTracker* allocate_tracker_locked();
    void release_task_locked(Task& task);
    void release_tracker_locked(TaskTracker& tracker);

    void link_tracker_locked(TaskTracker& tracker, TaskTracker* parent);
    void unlink_tracker_locked(TaskTracker& tracker);
    void remove_tracker_locked(TaskTracker& tracker);

    [[nodiscard]] TaskTracker* find_child_locked(const TaskTracker& parent, int64_t tid) const;

    Spinlock lock_;
    Task tasks_[kMaxTasks];
    TaskTracker trackers_[kMaxTasks];
    bool task_slots_used_[kMaxTasks];
    bool tracker_slots_used_[kMaxTasks];
    TaskTracker* root_tasks_;
    TaskId next_tid_;
    TaskId initial_task_id_;
    bool initialized_;
};

} // namespace jixia::microkernel::task
