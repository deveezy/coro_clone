#pragma once

#include <coroutine>
#include <exception>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace coro {
template <typename return_type = void>
class Task;
namespace detail {
struct PromiseBase {
  friend struct FinalAwaitable;
  struct FinalAwaitable {
    auto await_ready() const noexcept -> bool { return false; }

    template <typename promise_type>
    auto await_suspend(std::coroutine_handle<promise_type> coroutine) noexcept -> std::coroutine_handle<> {
      // If there is a continuation call it, otherwise this is the end of the line
      auto &promise = coroutine.promise();
      if (promise.continuation_ != nullptr) {
        return promise.continuation_;
      } else {
        return std::noop_coroutine();
      }
    }

    auto await_resume() noexcept -> void {}
  };

  PromiseBase() noexcept = default;
  ~PromiseBase()         = default;

  auto initial_suspend() noexcept { return std::suspend_always {}; }
  auto final_suspend() noexcept { return FinalAwaitable {}; }
  auto continuation(std::coroutine_handle<> continuation) noexcept -> void { continuation_ = continuation; }

protected:
  std::coroutine_handle<> continuation_ {nullptr};
};

template <typename return_type>
struct Promise final : public PromiseBase {
public:
  using task_type                                = Task<return_type>;
  using coroutine_handle                         = std::coroutine_handle<Promise<return_type>>;
  static constexpr bool return_type_is_reference = std::is_reference_v<return_type>;
  using stored_type  = std::conditional_t<return_type_is_reference, std::remove_reference_t<return_type> *,
       std::remove_const_t<return_type>>;
  using variant_type = std::variant<std::monostate, stored_type, std::exception_ptr>;

  auto get_return_object() noexcept -> task_type;

  template <typename value_type>
    requires(return_type_is_reference and std::is_constructible_v<return_type, value_type &&>) or
            (not return_type_is_reference and std::is_constructible_v<stored_type, value_type &&>)
  auto return_value(value_type &&value) -> void {
    if constexpr (return_type_is_reference) {
      return_type ref = static_cast<value_type &&>(value);
      storage_.template emplace<stored_type>(std::addressof(ref));
    } else {
      storage_.template emplace<stored_type>(std::forward<value_type>(value));
    }
  }

  auto return_value(stored_type &&value) -> void
    requires(not return_type_is_reference)
  {
    if constexpr (std::is_move_constructible_v<stored_type>) {
      storage_.template emplace<stored_type>(std::move(value));
    } else {
      storage_.template emplace<stored_type>(value);
    }
  }

  auto unhandled_exception() noexcept -> void { new (&storage_) variant_type(std::current_exception()); }

  auto result() & -> decltype(auto) {
    if (std::holds_alternative<stored_type>(storage_)) {
      if constexpr (return_type_is_reference) {
        return static_cast<return_type>(*std::get<stored_type>(storage_));
      } else {
        return static_cast<const return_type &>(std::get<stored_type>(storage_));
      }
    } else if (std::holds_alternative<std::exception_ptr>(storage_)) {
      std::rethrow_exception(std::get<std::exception_ptr>(storage_));
    } else {
      throw std::runtime_error {"The return value was never set, did you execute the coroutine?"};
    }
  }

  auto result() const & -> decltype(auto) {
    if (std::holds_alternative<stored_type>(storage_)) {
      if constexpr (return_type_is_reference) {
        return static_cast<std::add_const_t<return_type>>(*std::get<stored_type>(storage_));
      } else {
        return static_cast<const return_type &>(std::get<stored_type>(storage_));
      }
    } else if (std::holds_alternative<std::exception_ptr>(storage_)) {
      std::rethrow_exception(std::get<std::exception_ptr>(storage_));
    } else {
      throw std::runtime_error {"The return value was never set, did you execute the coroutine?"};
    }
  }

  auto result() && -> decltype(auto) {
    if (std::holds_alternative<stored_type>(storage_)) {
      if constexpr (return_type_is_reference) {
        return static_cast<return_type>(*std::get<stored_type>(storage_));
      } else if constexpr (std::is_move_constructible_v<return_type>) {
        return static_cast<return_type &&>(std::get<stored_type>(storage_));
      } else {
        return static_cast<const return_type &&>(std::get<stored_type>(storage_));
      }
    } else if (std::holds_alternative<std::exception_ptr>(storage_)) {
      std::rethrow_exception(std::get<std::exception_ptr>(storage_));
    } else {
      throw std::runtime_error {"The return value was never set, did you execute the coroutine?"};
    }
  }

private:
  variant_type storage_ {};
};

template <>
struct Promise<void> : public PromiseBase {
  using task_type                     = Task<void>;
  using coroutine_handle              = std::coroutine_handle<Promise<void>>;
  Promise() noexcept                  = default;
  Promise(const Promise &)            = delete;
  Promise(Promise &&other)            = delete;
  Promise &operator=(const Promise &) = delete;
  Promise &operator=(Promise &&other) = delete;
  ~Promise()                          = default;

  auto get_return_object() noexcept -> task_type;

  auto return_void() noexcept -> void {}

  auto unhandled_exception() noexcept -> void { exception_ = std::current_exception(); }

  auto result() -> void {
    if (exception_) { std::rethrow_exception(exception_); }
  }

private:
  std::exception_ptr exception_ {nullptr};
};
}  // namespace detail

template <typename return_type>
class [[nodiscard]] Task {
public:
  using task_type        = Task<return_type>;
  using promise_type     = detail::Promise<return_type>;
  using coroutine_handle = std::coroutine_handle<promise_type>;

  struct AwaitableBase {
    AwaitableBase(coroutine_handle coroutine) noexcept : coroutine_(coroutine) {}
    auto await_ready() const noexcept -> bool { return !coroutine_ || coroutine_.done(); }

    auto await_suspend(std::coroutine_handle<> awaitingCoroutine) noexcept -> std::coroutine_handle<> {
      coroutine_.promise().continuation(awaitingCoroutine);
      return coroutine_;
    }

    std::coroutine_handle<promise_type> coroutine_ {nullptr};
  };

  Task() noexcept = default;

  explicit Task(coroutine_handle handle) : coroutine_(handle) {}
  Task(const Task &) = delete;
  Task(Task &&other) noexcept : coroutine_(std::exchange(other.coroutine_, nullptr)) {}

  ~Task() {
    if (coroutine_ != nullptr) { coroutine_.destroy(); }
  }
  auto operator=(const Task &) -> Task & = delete;

  auto operator=(Task &&other) noexcept -> Task & {
    if (std::addressof(other) != this) {
      if (coroutine_ != nullptr) { coroutine_.destroy(); }

      coroutine_ = std::exchange(other.coroutine_, nullptr);
    }

    return *this;
  }

  /**
     * @return True if the task is in its final suspend or if the task has been destroyed.
     */
  auto is_ready() const noexcept -> bool { return coroutine_ == nullptr || coroutine_.done(); }

  auto resume() -> bool {
    if (!coroutine_.done()) { coroutine_.resume(); }
    return !coroutine_.done();
  }

  auto destroy() -> bool {
    if (coroutine_ != nullptr) {
      coroutine_.destroy();
      coroutine_ = nullptr;
      return true;
    }

    return false;
  }

  auto operator co_await() const & noexcept {
    struct Awaitable : public AwaitableBase {
      auto await_resume() -> decltype(auto) { return this->coroutine_.promise().result(); }
    };
    return Awaitable {coroutine_};
  }

  auto operator co_await() const && noexcept {
    struct awaitable : public AwaitableBase {
      auto await_resume() -> decltype(auto) { return std::move(this->coroutine_.promise()).result(); }
    };

    return awaitable {coroutine_};
  }
  //
  auto promise() & -> promise_type & { return coroutine_.promise(); }
  auto promise() const & -> const promise_type & { return coroutine_.promise(); }
  auto promise() && -> promise_type && { return std::move(coroutine_.promise()); }

  auto handle() -> coroutine_handle { return coroutine_; }

private:
  coroutine_handle coroutine_ {nullptr};
};

namespace detail {
template <typename return_type>
inline auto Promise<return_type>::get_return_object() noexcept -> Task<return_type> {
  return Task<return_type> {coroutine_handle::from_promise(*this)};
}

inline auto Promise<void>::get_return_object() noexcept -> Task<> {
  return Task<> {coroutine_handle::from_promise(*this)};
}

}  // namespace detail
}  // namespace coro
