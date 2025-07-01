#pragma once

#if !defined(AWAITABLE_IMPL_INL_H)
  #error "Do not include directly"
  #include <concepts>
  #include <coroutine>
  #include <type_traits>
  #include <utility>
#endif

namespace coro::concepts {

template <typename type, typename... types>
concept in_types = (std::same_as<type, types> || ...);
}
