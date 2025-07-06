#include <coro/task.hpp>

#include <chrono>
#include <iostream>
#include <thread>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

class TaskTest : public ::testing::Test {
  // protected:
  //   void SetUp() override {
  //     // Reset static counters before each test
  //     move_construct_only::move_count      = 0;
  //     copy_construct_only::copy_count      = 0;
  //     move_copy_construct_only::move_count = 0;
  //     move_copy_construct_only::copy_count = 0;
  //   }
};

TEST_F(TaskTest, StringTask) {
  using task_type = coro::Task<std::string>;

  auto h = []() -> task_type { co_return "Hello"; }();
  auto w = []() -> task_type { co_return "World"; }();

  ASSERT_THROW(h.promise().result(), std::runtime_error);
  ASSERT_THROW(w.promise().result(), std::runtime_error);

  h.resume();  // task suspends immediately
  w.resume();

  ASSERT_TRUE(h.is_ready());
  ASSERT_TRUE(w.is_ready());

  auto w_value = std::move(w).promise().result();
  ASSERT_EQ(w_value, "World");
  ASSERT_TRUE(w.promise().result().empty());
}

TEST_F(TaskTest, VoidTaskCompletesSuccessfully) {
  using task_type = coro::Task<>;

  auto t = []() -> task_type {
    std::this_thread::sleep_for(10ms);
    co_return;
  }();

  t.resume();

  EXPECT_TRUE(t.is_ready());
}

TEST_F(TaskTest, ExceptionThrown) {
  using task_type = coro::Task<std::string>;

  std::string throw_msg = "I'll be reached";

  auto task = [](std::string &throw_msg) -> task_type {
    throw std::runtime_error(throw_msg);
    co_return "I'll never be reached";
  }(throw_msg);

  task.resume();

  EXPECT_TRUE(task.is_ready());

  bool thrown = false;
  try {
    auto value = task.promise().result();
  } catch (const std::exception &e) {
    thrown = true;
    EXPECT_EQ(e.what(), throw_msg);
  }

  EXPECT_TRUE(thrown);
}

TEST_F(TaskTest, TaskInTask) {
  auto outer_task = []() -> coro::Task<> {
    auto inner_task = []() -> coro::Task<int> {
      std::cerr << "inner_task start\n";
      std::cerr << "inner_task stop\n";
      co_return 42;
    };

    std::cerr << "outer_task start\n";
    auto v = co_await inner_task();
    EXPECT_EQ(v, 42);
    std::cerr << "outer_task stop\n";
  }();

  outer_task.resume();
  EXPECT_TRUE(outer_task.is_ready());
}

TEST_F(TaskTest, NestedTasks) {
  auto task1 = []() -> coro::Task<> {
    std::cerr << "task1 start\n";
    auto task2 = []() -> coro::Task<int> {
      std::cerr << "\ttask2 start\n";
      auto task3 = []() -> coro::Task<int> {
        std::cerr << "\t\ttask3 start\n";
        std::cerr << "\t\ttask3 stop\n";
        co_return 3;
      };

      auto v2 = co_await task3();
      EXPECT_EQ(v2, 3);

      std::cerr << "\ttask2 stop\n";
      co_return 2;
    };

    auto v1 = co_await task2();
    EXPECT_EQ(v1, 2);

    std::cerr << "task1 stop\n";
  }();

  task1.resume();
  EXPECT_TRUE(task1.is_ready());
}

TEST_F(TaskTest, MultipleSuspendsReturnVoid) {
  auto task = []() -> coro::Task<void> {
    co_await std::suspend_always {};
    co_await std::suspend_never {};
    co_await std::suspend_always {};
    co_await std::suspend_always {};
    co_return;
  }();

  task.resume();
  EXPECT_FALSE(task.is_ready());

  task.resume();
  EXPECT_FALSE(task.is_ready());

  task.resume();
  EXPECT_FALSE(task.is_ready());

  task.resume();
  EXPECT_TRUE(task.is_ready());
}

TEST_F(TaskTest, MultipleSuspendsReturnInteger) {
  auto task = []() -> coro::Task<int> {
    co_await std::suspend_always {};
    co_await std::suspend_always {};
    co_await std::suspend_always {};
    co_return 11;
  }();

  task.resume();
  EXPECT_FALSE(task.is_ready());

  task.resume();
  EXPECT_FALSE(task.is_ready());

  task.resume();
  EXPECT_FALSE(task.is_ready());

  task.resume();
  EXPECT_TRUE(task.is_ready());
  EXPECT_EQ(task.promise().result(), 11);
}

TEST_F(TaskTest, ResumeFromPromiseToDifferentCoroutineHandles) {
  auto task1 = []() -> coro::Task<int> {
    std::cerr << "Task ran\n";
    co_return 42;
  }();

  auto task2 = []() -> coro::Task<void> {
    std::cerr << "Task 2 ran\n";
    co_return;
  }();

  std::vector<std::coroutine_handle<>> handles;

  auto &&t  = std::move(task1).promise();
  auto &&t2 = task1.promise();
  handles.emplace_back(std::coroutine_handle<coro::Task<int>::promise_type>::from_promise(task1.promise()));
  handles.emplace_back(std::coroutine_handle<coro::Task<void>::promise_type>::from_promise(task2.promise()));

  auto &coro_handle1 = handles[0];
  coro_handle1.resume();
  auto &coro_handle2 = handles[1];
  coro_handle2.resume();

  EXPECT_TRUE(task1.is_ready());
  EXPECT_TRUE(coro_handle1.done());
  EXPECT_EQ(task1.promise().result(), 42);

  EXPECT_TRUE(task2.is_ready());
  EXPECT_TRUE(coro_handle2.done());
}

TEST_F(TaskTest, ThrowsVoid) {
  auto task = []() -> coro::Task<void> {
    throw std::runtime_error {"I always throw."};
    co_return;
  }();

  EXPECT_NO_THROW(task.resume());
  EXPECT_TRUE(task.is_ready());
  EXPECT_THROW(task.promise().result(), std::runtime_error);
}

TEST_F(TaskTest, ThrowsNonVoidLValue) {
  auto task = []() -> coro::Task<int> {
    throw std::runtime_error {"I always throw."};
    co_return 42;
  }();

  EXPECT_NO_THROW(task.resume());
  EXPECT_TRUE(task.is_ready());
  EXPECT_THROW(task.promise().result(), std::runtime_error);
}

TEST_F(TaskTest, ThrowsNonVoidRValue) {
  struct type {
    int m_value;
  };

  auto task = []() -> coro::Task<type> {
    type return_value {42};
    throw std::runtime_error {"I always throw."};
    co_return std::move(return_value);
  }();

  task.resume();
  EXPECT_TRUE(task.is_ready());
  EXPECT_THROW(task.promise().result(), std::runtime_error);
}

TEST_F(TaskTest, ConstTaskReturnsReference) {
  struct type {
    int m_value;
  };

  type return_value {42};

  auto task = [](type &return_value) -> coro::Task<const type &> { co_return std::ref(return_value); }(return_value);

  task.resume();
  EXPECT_TRUE(task.is_ready());

  const auto &result = task.promise().result();
  EXPECT_EQ(result.m_value, 42);
  EXPECT_EQ(std::addressof(return_value), std::addressof(result));
  static_assert(std::is_same_v<decltype(task.promise().result()), const type &>);
}

TEST_F(TaskTest, MutableTaskReturnsReference) {
  struct type {
    int m_value;
  };

  type return_value {42};

  auto task = [](type &return_value) -> coro::Task<type &> { co_return std::ref(return_value); }(return_value);

  task.resume();
  EXPECT_TRUE(task.is_ready());

  auto &result = task.promise().result();
  EXPECT_EQ(result.m_value, 42);
  EXPECT_EQ(std::addressof(return_value), std::addressof(result));
  static_assert(std::is_same_v<decltype(task.promise().result()), type &>);
}

#if 0
TEST_F(TaskTest, NoDefaultConstructorRequired) {
  struct A {
    A(int value) : m_value(value) {}
    int m_value {};
  };

  auto make_task = []() -> coro::Task<A> { co_return A(42); };

  EXPECT_EQ(coro::sync_wait(make_task()).m_value, 42);
}

TEST_F(TaskTest, SupportsRvalueReference) {
  int i          = 42;
  auto make_task = [](int &i) -> coro::task<int &&> { co_return std::move(i); };

  int ret = coro::sync_wait(make_task(i));
  EXPECT_EQ(ret, 42);
}

struct move_construct_only {
  static int move_count;
  move_construct_only(int &i) : i(i) {}
  move_construct_only(move_construct_only &&x) noexcept : i(x.i) { ++move_count; }
  move_construct_only(const move_construct_only &)            = delete;
  move_construct_only &operator=(move_construct_only &&)      = delete;
  move_construct_only &operator=(const move_construct_only &) = delete;
  ~move_construct_only()                                      = default;
  int &i;
};

int move_construct_only::move_count = 0;

struct copy_construct_only {
  static int copy_count;
  copy_construct_only(int i) : i(i) {}
  copy_construct_only(copy_construct_only &&) = delete;
  copy_construct_only(const copy_construct_only &x) noexcept : i(x.i) { ++copy_count; }
  copy_construct_only &operator=(copy_construct_only &&)      = delete;
  copy_construct_only &operator=(const copy_construct_only &) = delete;
  ~copy_construct_only()                                      = default;
  int i;
};

int copy_construct_only::copy_count = 0;

struct move_copy_construct_only {
  static int move_count;
  static int copy_count;
  move_copy_construct_only(int i) : i(i) {}
  move_copy_construct_only(move_copy_construct_only &&x) noexcept : i(x.i) { ++move_count; }
  move_copy_construct_only(const move_copy_construct_only &x) noexcept : i(x.i) { ++copy_count; }
  move_copy_construct_only &operator=(move_copy_construct_only &&)      = delete;
  move_copy_construct_only &operator=(const move_copy_construct_only &) = delete;
  ~move_copy_construct_only()                                           = default;
  int i;
};

int move_copy_construct_only::move_count = 0;
int move_copy_construct_only::copy_count = 0;

TEST_F(TaskTest, SupportsNonAssignableTypes) {
  int i = 42;

  // Test move_construct_only
  auto move_task = [&i]() -> coro::task<move_construct_only> { co_return move_construct_only(i); };
  auto move_ret  = coro::sync_wait(move_task());
  EXPECT_EQ(std::addressof(move_ret.i), std::addressof(i));
  EXPECT_EQ(move_construct_only::move_count, 2);

  move_construct_only::move_count = 0;
  auto move_task2                 = [&i]() -> coro::task<move_construct_only> { co_return i; };
  auto move_ret2                  = coro::sync_wait(move_task2());
  EXPECT_EQ(std::addressof(move_ret2.i), std::addressof(i));
  EXPECT_EQ(move_construct_only::move_count, 1);

  // Test copy_construct_only
  auto copy_task = [&i]() -> coro::task<copy_construct_only> { co_return copy_construct_only(i); };
  auto copy_ret  = coro::sync_wait(copy_task());
  EXPECT_EQ(copy_ret.i, 42);
  EXPECT_EQ(copy_construct_only::copy_count, 2);

  copy_construct_only::copy_count = 0;
  auto copy_task2                 = [&i]() -> coro::task<copy_construct_only> { co_return i; };
  auto copy_ret2                  = coro::sync_wait(copy_task2());
  EXPECT_EQ(copy_ret2.i, 42);
  EXPECT_EQ(copy_construct_only::copy_count, 1);

  // Test move_copy_construct_only
  auto move_copy_task = [&i]() -> coro::task<move_copy_construct_only> { co_return move_copy_construct_only(i); };
  auto task           = move_copy_task();
  auto move_copy_ret1 = coro::sync_wait(task);
  auto move_copy_ret2 = coro::sync_wait(std::move(task));
  EXPECT_EQ(move_copy_ret1.i, 42);
  EXPECT_EQ(move_copy_ret2.i, 42);
  EXPECT_EQ(move_copy_construct_only::move_count, 2);
  EXPECT_EQ(move_copy_construct_only::copy_count, 1);

  // Test tuple return
  auto make_tuple_task = [](int i) -> coro::task<tuple<int, int>> { co_return make_tuple(i, i * 2); };
  auto tuple_ret       = coro::sync_wait(make_tuple_task(i));
  EXPECT_EQ(get<0>(tuple_ret), 42);
  EXPECT_EQ(get<1>(tuple_ret), 84);

  // Test reference return
  auto make_ref_task = [&i]() -> coro::task<int &> { co_return std::ref(i); };
  auto &ref_ret      = coro::sync_wait(make_ref_task());
  EXPECT_EQ(std::addressof(ref_ret), std::addressof(i));
}
#endif

TEST_F(TaskTest, PromiseSizeCheck) {
  EXPECT_GE(sizeof(coro::detail::Promise<void>), sizeof(std::coroutine_handle<>) + sizeof(std::exception_ptr));

  EXPECT_EQ(sizeof(coro::detail::Promise<int32_t>),
      sizeof(std::coroutine_handle<>) + sizeof(std::variant<int32_t, std::exception_ptr>));

  EXPECT_GE(sizeof(coro::detail::Promise<int64_t>),
      sizeof(std::coroutine_handle<>) + sizeof(std::variant<int64_t, std::exception_ptr>));

  EXPECT_GE(sizeof(coro::detail::Promise<std::vector<int64_t>>),
      sizeof(std::coroutine_handle<>) + sizeof(std::variant<std::vector<int64_t>, std::exception_ptr>));
}

TEST_F(TaskTest, TaskDestructor) {
  // Just a placeholder test for destructor behavior
  GTEST_SKIP() << "Destructor test is observational only";
}
