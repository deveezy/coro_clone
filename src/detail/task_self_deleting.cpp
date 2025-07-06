#include <coro/detail/task_self_deleting.hpp>

#include <coroutine>
#include <utility>

namespace coro::detail {
PromiseSelfDeleting::PromiseSelfDeleting()  = default;
PromiseSelfDeleting::~PromiseSelfDeleting() = default;

PromiseSelfDeleting::PromiseSelfDeleting(PromiseSelfDeleting &&other)
    : executorSize_(std::exchange(other.executorSize_, nullptr)) {}

auto PromiseSelfDeleting::operator=(PromiseSelfDeleting &&other) -> PromiseSelfDeleting & {
  if (std::addressof(other) != this) { executorSize_ = std::exchange(other.executorSize_, nullptr); }
  return *this;
}

auto PromiseSelfDeleting::get_return_object() -> TaskSelfDeleting { return TaskSelfDeleting(*this); }

auto PromiseSelfDeleting::initial_suspend() -> std::suspend_always { return std::suspend_always {}; }

auto PromiseSelfDeleting::final_suspend() noexcept -> std::suspend_never {
  // Notify the task_container<executor_t> that this coroutine has completed
  if (executorSize_ != nullptr) { executorSize_->fetch_sub(1, std::memory_order_release); }
  return std::suspend_never {};
}

auto PromiseSelfDeleting::return_void() noexcept -> void {
  // no-op
}

auto PromiseSelfDeleting::unhandled_exception() -> void {
  // The user cannot access the promise anyways, ignore the exception.
}

auto PromiseSelfDeleting::executor_size(std::atomic<std::size_t> &executor_size) -> void {
  executorSize_ = &executor_size;
}

TaskSelfDeleting::TaskSelfDeleting(PromiseSelfDeleting &promise) : promise_(&promise) {}

TaskSelfDeleting::~TaskSelfDeleting() {}

TaskSelfDeleting::TaskSelfDeleting(TaskSelfDeleting &&other) : promise_(other.promise_) {}

auto TaskSelfDeleting::operator=(TaskSelfDeleting &&other) -> TaskSelfDeleting & {
  if (std::addressof(other) != this) { promise_ = other.promise_; }

  return *this;
}

auto make_task_self_deleting(coro::Task<void> user_task) -> TaskSelfDeleting {
  co_await user_task;
  co_return;
}

}  // namespace coro::detail
