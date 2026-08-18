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

    if (task.queued) {
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
    if (task.state == task::TaskState::ended || task.idle || task.cpu == nullptr) {
        return false;
    }

    task.state = task::TaskState::ready;

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
        local.timeslice_ticks = time::TimeManager::instance().idle_timeslice_ticks();
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
