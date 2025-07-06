#pragma once

#include <atomic>
#include <coro/task.hpp>

namespace coro::detail {
class TaskSelfDeleting;

class PromiseSelfDeleting {
public:
  PromiseSelfDeleting();
  ~PromiseSelfDeleting();
  PromiseSelfDeleting(const PromiseSelfDeleting &) = delete;
  PromiseSelfDeleting(PromiseSelfDeleting &&);
  auto operator=(const PromiseSelfDeleting &) -> PromiseSelfDeleting & = delete;
  auto operator=(PromiseSelfDeleting &&) -> PromiseSelfDeleting &;

  auto get_return_object() -> TaskSelfDeleting;
  auto initial_suspend() -> std::suspend_always;
  auto final_suspend() noexcept -> std::suspend_never;
  auto return_void() noexcept -> void;
  auto unhandled_exception() -> void;

  auto executor_size(std::atomic<std::size_t> &taskContainerSize) -> void;

private:
  /**
     * The executor m_size member to decrement upon the coroutine completing.
     */
  std::atomic<std::size_t> *executorSize_ {nullptr};
};

/**
 * This task will self delete upon completing. This is useful for usecase that the lifetime of the
 * coroutine cannot be determined and it needs to 'self' delete. This is achieved by returning
 * std::suspend_never from the promise::final_suspend which then based on the spec tells the
 * coroutine to delete itself. This means any classes that use this task cannot have owning
 * pointers or relationships to this class and must not use it past its completion.
 *
 * This class is currently only used by coro::task_container<executor_t> and will decrement its
 * m_size internal count when the coroutine completes.
 */

class TaskSelfDeleting {
public:
  using promise_type = PromiseSelfDeleting;
  explicit TaskSelfDeleting(PromiseSelfDeleting &promise);
  ~TaskSelfDeleting();

  TaskSelfDeleting(const TaskSelfDeleting &) = delete;
  TaskSelfDeleting(TaskSelfDeleting &&);
  auto operator=(const TaskSelfDeleting &) -> TaskSelfDeleting & = delete;
  auto operator=(TaskSelfDeleting &&) -> TaskSelfDeleting &;
  auto promise() -> PromiseSelfDeleting & { return *promise_; }
  auto handle() -> std::coroutine_handle<PromiseSelfDeleting> {
    return std::coroutine_handle<PromiseSelfDeleting>::from_promise(*promise_);
  }

  auto resume() -> bool {
    auto h = handle();
    if (!h.done()) { h.resume(); }
    return !h.done();
  }

private:
  PromiseSelfDeleting *promise_ {nullptr};
};

auto makeTaskSelfDeleting(coro::Task<void> userTask) -> TaskSelfDeleting;
};  // namespace coro::detail
