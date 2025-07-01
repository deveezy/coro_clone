#include <iostream>

#include <coro/thread_pool.hpp>
#include <coro/task.hpp>

std::string global = "check";
coro::Task<int> foo() { co_return 1448; }
coro::Task<std::string &> foo2() { co_return global; }

int main() {
  std::cout << "Hello, world" << std::endl;
  auto r          = foo();
  const auto &res = std::move(r).promise().result();
  auto rr         = foo2().promise().result();
  return 0;
}
