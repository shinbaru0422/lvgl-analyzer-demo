#include "infra/runtime/UiTaskQueue.h"

#include <utility>

namespace infra::runtime {

void UiTaskQueue::post(std::function<void()> task) {
    std::lock_guard lock(mutex_);
    pending_.push_back(std::move(task));
}

std::size_t UiTaskQueue::drain() {
    std::vector<std::function<void()>> tasks;
    {
        std::lock_guard lock(mutex_);
        tasks.swap(pending_);
    }
    for (auto& task : tasks) task();
    return tasks.size();
}

}  // namespace infra::runtime
