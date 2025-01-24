#pragma once

#include "utils/utils.h"
#include <array>
#include <new>
#include <typeinfo>
#include <utility>

#define fwd(x) static_cast<decltype(x) &&>(x)

namespace erased {

struct Copy {
  template <typename T>
  static constexpr void *invoker(const T &object, void *sooPtr,
                                 std::size_t sooSize) {
    if (sizeof object > sooSize || std::is_constant_evaluated())
      return new T{object};
    else {
      return new (sooPtr) T{object};
    }
  }
};

struct Move {
  template <typename T>
  static constexpr void *invoker(T &object, void *sooPtr, std::size_t sooSize) {
    if (sizeof object > sooSize || std::is_constant_evaluated())
      return new T{std::move(object)};
    else {
      return new (sooPtr) T{std::move(object)};
    }
  }
};

namespace details {
template <typename Soo> struct Destructor {
  template <typename T> static constexpr void invoker(T &, Soo *soo) {
    soo->template destroy<T>();
  }
};

template <int Size, typename... Methods> struct soo {
  using vtable = details::vtable<Methods..., Destructor<soo>>;
  static constexpr auto buffer_size =
      Size - sizeof(void *) - sizeof(const vtable *);
  std::array<std::byte, buffer_size> m_buffer;
  void *ptr = nullptr;
  const vtable *vtable_ptr = nullptr;

  template <typename T, typename... Args>
  constexpr T *construct(Args &&...args) {
    static_assert(sizeof(soo) == Size);
    if constexpr (sizeof(T) <= buffer_size) {
      if (std::is_constant_evaluated()) {
        ptr = new T{fwd(args)...};
      } else {
        ptr = new (m_buffer.data()) T{fwd(args)...};
      }
    } else {
      ptr = new T{fwd(args)...};
    }
    vtable_ptr = vtable::template construct_for<T>();
    return static_cast<T *>(ptr);
  }

  template <typename T> constexpr T *get() noexcept {
    return static_cast<T *>(ptr);
  }

  template <typename T> constexpr const T *get() const noexcept {
    return static_cast<const T *>(ptr);
  }

  template <typename T> constexpr void destroy() noexcept {
    T *ptr = get<T>();
    if constexpr (sizeof(T) <= buffer_size) {
      if (std::is_constant_evaluated()) {
        delete ptr;
      } else {
        ptr->~T();
      }
    } else {
      delete ptr;
    }
  }
};

template <typename T, typename... List> constexpr bool contains() {
  return (std::is_same_v<T, List> || ...);
}
} // namespace details

template <int Size, typename... Methods> struct basic_erased;

template <typename T> struct is_erased : std::false_type {};

template <int Size, typename... Methods>
struct is_erased<basic_erased<Size, Methods...>> : std::true_type {};

template <typename T> constexpr bool is_erased_v = is_erased<T>::value;

template <typename T>
concept erased_concept = is_erased_v<std::decay_t<T>>;

template <int Size, typename... Methods>
struct alignas(Size) basic_erased : public Methods... {
  using soo = details::soo<Size, Methods...>;

  static constexpr bool copyable = details::contains<Copy, Methods...>();
  static constexpr bool movable = details::contains<Move, Methods...>();

  template <typename T>
  constexpr basic_erased(std::in_place_type_t<T>, auto &&...args) noexcept {
    m_soo.template construct<T>(fwd(args)...);
  }

  template <typename T>
  constexpr basic_erased(T x) noexcept
      : basic_erased{std::in_place_type<T>, static_cast<T &&>(x)} {}

  template <typename Method>
  constexpr decltype(auto) invoke(Method, auto &&...xs) const {
    return m_soo.vtable_ptr->template get<Method>()(
        static_cast<const void *>(m_soo.ptr), fwd(xs)...);
  }

  template <typename Method>
  constexpr decltype(auto) invoke(Method, auto &&...xs) {
    return m_soo.vtable_ptr->template get<Method>()(m_soo.ptr, fwd(xs)...);
  }

  constexpr basic_erased(basic_erased &&other) noexcept
    requires movable
  {
    m_soo.vtable_ptr = other.m_soo.vtable_ptr;
    m_soo.ptr = other.invoke(Move{}, static_cast<void *>(m_soo.m_buffer.data()),
                             soo::buffer_size);
  }

  constexpr basic_erased &operator=(basic_erased &&other) noexcept
    requires movable
  {
    destroy();
    m_soo.vtable_ptr = other.m_soo.vtable_ptr;
    m_soo.ptr = other.invoke(Move{}, static_cast<void *>(m_soo.m_buffer.data()),
                             soo::buffer_size);
    return *this;
  }

  constexpr basic_erased(const basic_erased &other)
    requires copyable
  {
    m_soo.vtable_ptr = other.m_soo.vtable_ptr;
    m_soo.ptr = other.invoke(Copy{}, static_cast<void *>(m_soo.m_buffer.data()),
                             soo::buffer_size);
  }

  constexpr basic_erased &operator=(const basic_erased &other)
    requires copyable
  {
    destroy();
    m_soo.vtable_ptr = other.m_soo.vtable_ptr;
    m_soo.ptr = other.invoke(Copy{}, static_cast<void *>(m_soo.m_buffer.data()),
                             soo::buffer_size);
    return *this;
  }

  constexpr void destroy() {
    invoke(details::Destructor<decltype(m_soo)>{}, &m_soo);
  }

  constexpr ~basic_erased() { destroy(); }

  template <typename T, int S, typename... M>
  friend constexpr bool is(const basic_erased<S, M...> &object);

  template <typename T, erased_concept Erased>
  friend constexpr auto *any_cast(Erased *object);

  template <typename T, erased_concept Erased>
  friend constexpr auto &&any_cast(Erased &&object);

private:
  soo m_soo;
};

template <typename... Methods> using erased = basic_erased<32, Methods...>;

template <typename T, int Size, typename... Methods>
constexpr bool is(const basic_erased<Size, Methods...> &object) {
  using soo = typename basic_erased<Size, Methods...>::soo;
  using vtable = typename soo::vtable;

  return object.m_soo.vtable_ptr == vtable::template construct_for<T>();
}

template <typename T, erased_concept Erased>
constexpr auto *any_cast(Erased *object) {
  if (is<T>(*object))
    return object->m_soo.template get<T>();
  return decltype(object->m_soo.template get<T>()){nullptr};
}

template <typename T, erased_concept Erased>
constexpr auto &&any_cast(Erased &&object) {
  if (is<T>(object))
    return std::forward_like<Erased>(*object.m_soo.template get<T>());
  throw std::bad_cast();
}
} // namespace erased

#undef fwd

#define ERASED_HEAD(a, ...) a
#define ERASED_TAIL(a, ...) __VA_ARGS__
#define ERASED_EAT(...)
#define ERASED_EXPAND(...) __VA_ARGS__

#define ERASED_REMOVE_PARENTHESIS_IMPL(...) __VA_ARGS__
#define ERASED_REMOVE_PARENTHESIS(...)                                         \
  ERASED_EXPAND(ERASED_REMOVE_PARENTHESIS_IMPL __VA_ARGS__)

#define ERASED_ADD_COMA_AFTER_PARENTHESIS_IMPL(...) (__VA_ARGS__),
#define ERASED_ADD_COMA_AFTER_PARENTHESIS(...)                                 \
  (ERASED_EXPAND(ERASED_ADD_COMA_AFTER_PARENTHESIS_IMPL __VA_ARGS__))

#define ERASED_REMOVE_AFTER_PARENTHESIS(...)                                   \
  ERASED_REMOVE_PARENTHESIS(                                                   \
      ERASED_HEAD ERASED_ADD_COMA_AFTER_PARENTHESIS(__VA_ARGS__))

#define ERASED_CAT_IMPL(a, b) a##b
#define ERASED_CAT(...) ERASED_CAT_IMPL(__VA_ARGS__)
#define ERASED_GET_AFTER_requires(...) (__VA_ARGS__)
#define ERASED_GET_INSIDE_REQUIRES(...)                                        \
  ERASED_REMOVE_AFTER_PARENTHESIS(                                             \
      ERASED_CAT(ERASED_GET_AFTER_, ERASED_EAT __VA_ARGS__))

#define ERASED_GET_TRAILING_RETURN(...)                                        \
  ERASED_EXPAND(ERASED_TAIL ERASED_ADD_COMA_AFTER_PARENTHESIS(                 \
      ERASED_CAT(ERASED_GET_AFTER_, ERASED_EAT __VA_ARGS__)))

#define ERASED_MAKE_BEHAVIOR(Name, name, signature)                            \
  struct Name {                                                                \
    static constexpr auto                                                      \
    invoker(auto ERASED_REMOVE_AFTER_PARENTHESIS(signature))                   \
        ERASED_GET_TRAILING_RETURN(signature) {                                \
      return ERASED_GET_INSIDE_REQUIRES(signature);                            \
    }                                                                          \
                                                                               \
    constexpr decltype(auto) name(this auto &&self, auto &&...args) {          \
      return self.invoke(Name{}, static_cast<decltype(args) &&>(args)...);     \
    }                                                                          \
  }

#undef fwd
