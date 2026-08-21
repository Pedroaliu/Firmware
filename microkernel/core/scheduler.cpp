#include "microkernel/core/scheduler.h"

#include "microkernel/core/hart.h"
#include "microkernel/core/singleton.h"
#include "microkernel/core/task.h"
#include "microkernel/core/task_manager.h"
#include "microkernel/core/time_manager.h"

namespace jixia::microkernel::scheduler {

RunQueue::RunQueue() : head_(nullptr), tail_(nullptr), size_(0U) {
}

void RunQueue::reset() {
    SpinlockGuard guard(lock_);
    head_ = nullptr;
    tail_ = nullptr;
    size_ = 0U;
}

bool RunQueue::insert(task::Task& task) {
    SpinlockGuard guard(lock_);

    /*
     * M00-08.03.02: a task queued in an endpoint waiting FIFO is blocked and
     * must never enter a run queue; the FIFO unlink happens before add_task.
     */
    if (task.queued || task.delay.queued || task.message_wait.queued ||
        task.state == task::TaskState::ended) {
        return false;
    }

    task.previous = tail_;
    task.next = nullptr;

    if (tail_ != nullptr) {
        tail_->next = &task;
    } else {
        head_ = &task;
    }

    tail_ = &task;
    /*
     * Publish READY inside the same critical section that marks the task
     * queued. Consumers must hold this lock to dequeue, so a task can never
     * be observed in a run queue while its state is not READY, and a failed
     * insertion returns without touching the task (previous state kept).
     */
    task.state = task::TaskState::ready;
    task.queued = true;
    ++size_;
    return true;
}

task::Task* RunQueue::remove() {
    SpinlockGuard guard(lock_);

    task::Task* selected = head_;
    if (selected == nullptr) {
        return nullptr;
    }

    head_ = selected->next;
    if (head_ != nullptr) {
        head_->previous = nullptr;
    } else {
        tail_ = nullptr;
    }

    selected->previous = nullptr;
    selected->next = nullptr;
    selected->queued = false;
    --size_;

    if (selected->state != task::TaskState::ready || selected->delay.queued ||
        selected->message_wait.queued) {
        hart::park();
    }
    return selected;
}

size_t RunQueue::size() const {
    SpinlockGuard guard(lock_);
    return size_;
}

Scheduler::Scheduler() = default;

Scheduler& Scheduler::instance() {
    return Singleton<Scheduler>::instance();
}

void Scheduler::initialize() {
    global_run_queue_.reset();
    for (RunQueue& queue : local_run_queues_) {
        queue.reset();
    }
}

void Scheduler::bind_hart(hart::HartLocal& local) {
    if (local.index >= hart::kMaxHarts) {
        hart::park();
    }

    local.scheduler = this;
    local.scheduler_extra = &local_run_queues_[local.index];
}

bool Scheduler::add_task(task::Task& task) {
    /*
     * Every RunQueue::insert failure condition is checked before the READY
     * transition. Insert can then only fail on a concurrent-insert race, and
     * in that case it returns without mutating the task (fail-closed).
     * message_wait.queued (M00-08.03.02): a blocked receiver is unlinked from
     * its endpoint waiting FIFO before any add_task, so this only fires on a
     * broken invariant; callers such as IPC wake treat it as fail-closed.
     */
    if (task.state == task::TaskState::ended || task.idle || task.cpu == nullptr ||
        task.delay.queued || task.queued || task.message_wait.queued) {
        return false;
    }

    if (task.affinity_pinned != 0U) {
        auto* queue = static_cast<RunQueue*>(task.cpu->scheduler_extra);
        return queue != nullptr && queue->insert(task);
    }

    return global_run_queue_.insert(task);
}

void Scheduler::return_runnable() {
    task::Task* current = task::TaskManager::get_current_task();
    if (current != nullptr && !current->idle && !add_task(*current)) {
        hart::park();
    }
}

task::Task& Scheduler::set_next_runnable() {
    hart::HartLocal& local = hart::current();
    task::Task* selected = nullptr;

    auto* local_queue = static_cast<RunQueue*>(local.scheduler_extra);
    if (local_queue != nullptr) {
        selected = local_queue->remove();
    }

    if (selected == nullptr && global_run_queue_.size() != 0U) {
        selected = global_run_queue_.remove();
    }

    if (selected == nullptr) {
        selected = local.idle_task;
        local.timeslice_ticks = time::TimeManager::instance().idle_timeslice_ticks(local);
    } else {
        local.timeslice_ticks = time::TimeManager::instance().task_timeslice_ticks();
    }

    if (selected == nullptr) {
        hart::park();
    }

    task::TaskManager::set_current_task(*selected);
    return *selected;
}

} // namespace jixia::microkernel::scheduler
