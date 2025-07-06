#include <atomic>
#include <coro/thread_pool.hpp>
#include <coro/detail/task_self_deleting.hpp>
#include <stdexcept>

namespace coro {
ThreadPool::ScheduleOperation::ScheduleOperation(ThreadPool &_tp) noexcept : threadPool_(_tp) {}

auto ThreadPool::ScheduleOperation::await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept -> void {
  threadPool_.schedule_impl(awaitingCoroutine);
}

ThreadPool::ThreadPool(Options &&opts, PrivateConstructor) : opts_(opts) { threads_.reserve(opts_.threadCount_); }

auto ThreadPool::makeShared(Options opts) -> std::shared_ptr<ThreadPool> {
  auto tp = std::make_shared<ThreadPool>(std::move(opts), PrivateConstructor {});
  // Initialize once the shared pointer is constructor so it can be captured for
  // the background threads.
  for (uint32_t i = 0; i < tp->opts_.threadCount_; ++i) { tp->executor(i); }
  return tp;
}
ThreadPool::~ThreadPool() { shutdown(); }

auto ThreadPool::schedule() -> ScheduleOperation {
  size_.fetch_add(1, std::memory_order::release);
  if (!shutdownRequested_.load(std::memory_order::acquire)) {
    return ScheduleOperation {*this};
  } else {
    size_.fetch_sub(1, std::memory_order::release);
    throw std::runtime_error("coro::thread_pool is shutting down, unable to schedule new tasks");
  }
}

auto ThreadPool::spawn(coro::Task<void> &&task) noexcept -> bool {
  size_.fetch_add(1, std::memory_order::release);
  auto wrapperTask = detail::makeTaskSelfDeleting(std::move(task));
  wrapperTask.promise().executor_size(size_);
  return resume(wrapperTask.handle());
}

auto ThreadPool::resume(std::coroutine_handle<> handle) noexcept -> bool {
  if (handle == nullptr || handle.done()) { return false; }
  size_.fetch_add(1, std::memory_order::release);
  if (shutdownRequested_.load(std::memory_order_acquire)) {
    size_.fetch_sub(1, std::memory_order::release);
    return false;
  }
  schedule_impl(handle);
  return true;
}

auto ThreadPool::shutdown() noexcept -> void {
  if (shutdownRequested_.exchange(true, std::memory_order::acq_rel) == false) {
    {
      // There is a race condition if we are not holding the lock with the executors
      // to always receive this.  std::jthread stop token works without this properly.
      std::unique_lock lk {waitMutex_};
      waitCv_.notify_all();
    }

    for (auto &thread : threads_) {
      if (thread.joinable()) { thread.join(); }
    }
  }
}

auto ThreadPool::executor(std::size_t idx) -> void {
  if (opts_.onThreadStart_) { opts_.onThreadStart_(idx); }

  // Process until shutdown is requested
  while (!shutdownRequested_.load(std::memory_order::acquire)) {
    std::unique_lock lk {waitMutex_};
    waitCv_.wait(lk, [&]() { return !queue_.empty() || shutdownRequested_.load(std::memory_order::acquire); });

    if (queue_.empty()) { continue; }

    auto handle = queue_.front();
    queue_.pop_front();
    lk.unlock();

    // Release the lock while executing the coroutine
    handle.resume();
    size_.fetch_sub(1, std::memory_order::release);
  }

  // Process until there are no ready tasks left
  while (size_.load(std::memory_order::acquire)) {
    std::unique_lock lk {waitMutex_};
    // size_ will only drop to zero once all executing coroutines are finished
    // but the queue could be empty for threads that finished early

    if (queue_.empty()) { break; }

    auto handle = queue_.front();
    queue_.pop_front();
    lk.unlock();

    // Release the lock while executing the coroutine
    handle.resume();
    size_.fetch_sub(1, std::memory_order::release);
  }

  if (opts_.onThreadStop_) { opts_.onThreadStop_(idx); }
}

auto ThreadPool::schedule_impl(std::coroutine_handle<> handle) noexcept -> void {
  if (handle == nullptr || handle.done()) { return; }
  {
    std::scoped_lock lk(waitMutex_);
    queue_.emplace_back(handle);
    waitCv_.notify_one();
  }
}
}  // namespace coro
