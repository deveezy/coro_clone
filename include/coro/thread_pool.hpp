#pragma once
#include <coroutine>
#include <deque>
#include <functional>
#include <memory>
#include <ranges>
#include <thread>
#include <coro/task.hpp>

#define RANGE_OF_IMPL_INL_H
#include <coro/concepts/range_of.hpp>
#include <atomic>
#include <condition_variable>
#include <mutex>

namespace coro {
/**
 * Creates a thread pool that executes arbitrary coroutine tasks in a FIFO scheduler policy.
 * The thread pool by default will create an execution thread per available core on the system.
 *
 * When shutting down, either by the thread pool destructing or by manually calling shutdown()
 * the thread pool will stop accepting new tasks but will complete all tasks that were scheduled
 * prior to the shutdown request.
 */

class ThreadPool final : public std::enable_shared_from_this<ThreadPool> {
  struct PrivateConstructor {
    PrivateConstructor() = default;
  };

public:
  /**
    * A schedule operation is an awaitable type with a coroutine to resume the task scheduled on one of
    * the executor threads.
    */
  class ScheduleOperation {
    friend class ThreadPool;
    /**
      * Only thread_pools can create schedule operations when a task is being scheduled.
      * @param tp The thread pool that created this schedule operation.
      */
    explicit ScheduleOperation(ThreadPool &_tp) noexcept;

  public:
    /**
      * Schedule operations always pause so the executing thread can be switched.
    */
    auto await_ready() noexcept -> bool { return false; }

    /**
      * Suspending always returns to the caller (using void return of await_suspend()) and
      * stores the coroutine internally for the executing thread to resume from.
    */
    auto await_suspend(std::coroutine_handle<> awaiting_coroutine) noexcept -> void;

    /**
      * no-op as this is the function called first by the thread pool's executing thread.
    */
    auto await_resume() noexcept -> void {}

  private:
    ThreadPool &threadPool_;
  };

  struct Options {
    /// The number of executor threads for this thread pool.  Uses the hardware concurrency
    /// value by default.
    uint32_t threadCount_ = std::thread::hardware_concurrency();
    /// Functor to call on each executor thread upon starting execution.  The parameter is the
    /// thread's ID assigned to it by the thread pool.
    std::function<void(std::size_t)> onThreadStart_ = nullptr;
    /// Functor to call on each executor thread upon stopping execution.  The parameter is the
    /// thread's ID assigned to it by the thread pool.
    std::function<void(std::size_t)> onThreadStop_ = nullptr;
  };

  /**
     * @see thread_pool::make_shared
     */
  explicit ThreadPool(Options &&opts, PrivateConstructor);

  /**
     * @brief Creates a thread pool executor.
     *
     * @param opts The thread pool's options.
     * @return std::shared_ptr<thread_pool>
     */
  static auto makeShared(Options opts = Options {.threadCount_ = std::thread::hardware_concurrency(),
                             .onThreadStart_                   = nullptr,
                             .onThreadStop_                    = nullptr}) -> std::shared_ptr<ThreadPool>;
  ThreadPool(const ThreadPool &)                     = delete;
  ThreadPool(ThreadPool &&)                          = delete;
  auto operator=(const ThreadPool &) -> ThreadPool & = delete;
  auto operator=(ThreadPool &&) -> ThreadPool &      = delete;

  ~ThreadPool();

  /**
   * @return The number of executor threads for processing tasks.
  */
  auto threadCount() const noexcept -> size_t { return threads_.size(); }

  /**
     * Schedules the currently executing coroutine to be run on this thread pool.  This must be
     * called from within the coroutines function body to schedule the coroutine on the thread pool.
     * @throw std::runtime_error If the thread pool is `shutdown()` scheduling new tasks is not permitted.
     * @return The schedule operation to switch from the calling scheduling thread to the executor thread
     * pool thread.
     */
  [[nodiscard]] auto schedule() -> ScheduleOperation;

  /**
     * Spawns the given task to be run on this thread pool, the task is detached from the user.
     * @param task The task to spawn onto the thread pool.
     * @return True if the task has been spawned onto this thread pool.
     */
  auto spawn(coro::Task<void> &&task) noexcept -> bool;

  /**
     * Schedules a task on the thread pool and returns another task that must be awaited on for completion.
     * This can be done via co_await in a coroutine context or coro::sync_wait() outside of coroutine context.
     * @tparam return_type The return value of the task.
     * @param task The task to schedule on the thread pool.
     * @return The task to await for the input task to complete.
     */
  template <typename return_type>
  [[nodiscard]] auto schedule(coro::Task<return_type> task) -> coro::Task<return_type> {
    co_await schedule();
    co_return co_await task;
  }
  /**
     * Schedules any coroutine handle that is ready to be resumed.
     * @param handle The coroutine handle to schedule.
     * @return True if the coroutine is resumed, false if its a nullptr or the coroutine is already done.
     */
  auto resume(std::coroutine_handle<> handle) noexcept -> bool;
  /**
     * Schedules the set of coroutine handles that are ready to be resumed.
     * @param handles The coroutine handles to schedule.
     * @param uint64_t The number of tasks resumed, if any where null they are discarded.
     */
  template <coro::concepts::range_of<std::coroutine_handle<>> range_type>
  auto resume(const range_type &handles) noexcept -> uint64_t {
    size_.fetch_add(std::size(handles), std::memory_order::release);

    size_t null_handles {0};

    {
      std::scoped_lock lk {waitMutex_};
      for (const auto &handle : handles) {
        if (handle != nullptr) [[likely]] {
          queue_.emplace_back(handle);
        } else {
          ++null_handles;
        }
      }
    }

    if (null_handles > 0) { size_.fetch_sub(null_handles, std::memory_order::release); }

    uint64_t total = std::size(handles) - null_handles;
    if (total >= threads_.size()) {
      waitCv_.notify_all();
    } else {
      for (uint64_t i = 0; i < total; ++i) { waitCv_.notify_one(); }
    }

    return total;
  }

  /**
     * Immediately yields the current task and places it at the end of the queue of tasks waiting
     * to be processed.  This will immediately be picked up again once it naturally goes through the
     * FIFO task queue.  This function is useful to yielding long processing tasks to let other tasks
     * get processing time.
     */
  [[nodiscard]] auto yield() -> ScheduleOperation { return schedule(); }

  /**
     * Shutsdown the thread pool.  This will finish any tasks scheduled prior to calling this
     * function but will prevent the thread pool from scheduling any new tasks.  This call is
     * blocking and will wait until all inflight tasks are completed before returnin.
     */
  auto shutdown() noexcept -> void;

  /**
     * @return The number of tasks waiting in the task queue + the executing tasks.
     */
  auto size() const noexcept -> std::size_t { return size_.load(std::memory_order::acquire); }

  /**
     * @return True if the task queue is empty and zero tasks are currently executing.
     */
  auto empty() const noexcept -> bool { return size() == 0; }

  /**
     * @return The number of tasks waiting in the task queue to be executed.
     */
  auto queue_size() const noexcept -> std::size_t {
    std::atomic_thread_fence(std::memory_order::acquire);
    return queue_.size();
  }

  /**
     * @return True if the task queue is currently empty.
     */
  auto queue_empty() const noexcept -> bool { return queue_size() == 0; }

private:
  Options opts_;
  std::vector<std::thread> threads_;
  std::mutex waitMutex_;
  std::condition_variable_any waitCv_;
  std::deque<std::coroutine_handle<>> queue_;

  /**
     * Each background thread runs from this function.
     * @param idx The executor's idx for internal data structure accesses.
     */
  auto executor(std::size_t idx) -> void;

  /**
     * @param handle Schedules the given coroutine to be executed upon the first available thread.
     */
  auto schedule_impl(std::coroutine_handle<> handle) noexcept -> void;
  /// The number of tasks in the queue + currently executing.
  std::atomic<std::size_t> size_ {0};
  /// Has the thread pool been requested to shut down?
  std::atomic<bool> shutdownRequested_ {false};
};
}  // namespace coro
