#pragma once

#include <new>
#include <type_traits>
#include <utility>

#define fwd(x) static_cast<decltype(x) &&>(x)

namespace erased {

struct erased_type_t {};

template <bool condition> struct fast_conditional {
  template <typename T, typename F> using apply = T;
};

template <> struct fast_conditional<false> {
  template <typename T, typename F> using apply = F;
};

template <typename Method, typename Signature> struct MethodTraitImpl;

template <typename Method, typename ReturnType, typename ErasedType,
          typename... Args>
struct MethodTraitImpl<Method, ReturnType (*)(ErasedType &, Args...)> {
  using function =
      fast_conditional<std::is_const_v<ErasedType>>::template apply<
          ReturnType(Args...) const, ReturnType(Args...)>;
};

template <typename Method>
using MethodTrait =
    MethodTraitImpl<Method,
                    decltype(&Method::template invoker<erased::erased_type_t>)>;

template <typename Method>
using MethodPtr = typename MethodTrait<Method>::function;

template <typename Method, typename F> struct Signature;

template <typename Method>
using CreateSignature = Signature<Method, MethodPtr<Method>>;

struct Copy {
  static constexpr void invoker(const auto &) {}
};
struct Move {
  static constexpr void invoker(const auto &) {}
};

namespace details::base {
template <typename...> struct base_with_methods;

template <> struct base_with_methods<> {
  constexpr void invoker() {}
  constexpr virtual ~base_with_methods() = default;
};

template <typename Method, typename R, typename... Args, typename... Others>
struct base_with_methods<Signature<Method, R(Args...) const>, Others...>
    : base_with_methods<Others...> {
  using base_with_methods<Others...>::invoker;
  constexpr virtual R invoker(Method, Args...) const = 0;
};

template <typename Method, typename R, typename... Args, typename... Others>
struct base_with_methods<Signature<Method, R(Args...)>, Others...>
    : base_with_methods<Others...> {
  using base_with_methods<Others...>::invoker;
  constexpr virtual R invoker(Method, Args...) = 0;
};

} // namespace details::base

namespace details::concrete {
template <typename Base, typename Type, typename... Ts> struct concrete_method;

template <typename Base, typename Type>
struct concrete_method<Base, Type> : Base {
  Type m_object;

  constexpr concrete_method(auto &&...args) : m_object{fwd(args)...} {}
};

template <typename Base, typename Type, typename Method, typename R,
          typename... Args, typename... Others>
struct concrete_method<Base, Type, Signature<Method, R(Args...) const>,
                       Others...> : concrete_method<Base, Type, Others...> {
  using concrete_method<Base, Type, Others...>::concrete_method;
  using concrete_method<Base, Type, Others...>::invoker;

  constexpr R invoker(Method, Args... args) const override {
    return Method::invoker(this->m_object, fwd(args)...);
  }
};

template <typename Base, typename Type, typename Method, typename R,
          typename... Args, typename... Others>
struct concrete_method<Base, Type, Signature<Method, R(Args...)>, Others...>
    : concrete_method<Base, Type, Others...> {
  using concrete_method<Base, Type, Others...>::concrete_method;
  using concrete_method<Base, Type, Others...>::invoker;

  constexpr R invoker(Method, Args... args) override {
    return Method::invoker(this->m_object, fwd(args)...);
  }
};

} // namespace details::concrete
static_assert(sizeof(bool) == 1);

template <typename T, int N> constexpr bool is_dynamic() {
  if constexpr (sizeof(T) <= N)
    return std::is_constant_evaluated();
  else
    return true;
}

template <typename T, std::size_t N>
constexpr auto *construct(char (&array)[N], auto &&...args) {
  if constexpr (sizeof(T) <= N) {
    if (std::is_constant_evaluated())
      return new T{fwd(args)...};
    return new (array) T{fwd(args)...};
  } else {
    return new T{fwd(args)...};
  }
}

template <typename T, typename... List> constexpr bool contains() {
  return (std::is_same_v<T, List> || ...);
}

template <int Size, typename... Methods>
struct alignas(Size) basic_erased : public Methods... {
  static constexpr bool copyable = contains<Copy, Methods...>();
  static constexpr bool movable = contains<Move, Methods...>();

  struct Base : details::base::base_with_methods<CreateSignature<Methods>...> {
    constexpr virtual Base *clone(bool isDynamic, char *buffer) const = 0;
    constexpr virtual Base *move(bool isDynamic, char *buffer) noexcept = 0;
  };

  static constexpr auto buffer_size = Size - sizeof(bool) - sizeof(Base *);

  template <typename Type>
  struct concrete
      : details::concrete::concrete_method<Base, Type,
                                           CreateSignature<Methods>...> {
    using details::concrete::concrete_method<
        Base, Type, CreateSignature<Methods>...>::concrete_method;

    constexpr virtual Base *clone(bool is_dynamic,
                                  char *soo_buffer) const override {
      if constexpr (copyable) {
        if (is_dynamic)
          return new concrete{this->m_object};
        return new (soo_buffer) concrete{this->m_object};
      } else {
        return nullptr;
      }
    }

    constexpr virtual Base *move(bool is_dynamic,
                                 char *soo_buffer) noexcept override {
      if constexpr (movable) {
        if (is_dynamic)
          return new concrete{static_cast<Type &&>(this->m_object)};
        return new (soo_buffer) concrete{static_cast<Type &&>(this->m_object)};
      } else {
        return nullptr;
      }
    }
  };

  char m_array[buffer_size];
  bool m_dynamic;
  Base *m_ptr;

  template <typename T>
  constexpr basic_erased(std::in_place_type_t<T>, auto &&...args) noexcept
      : m_dynamic{is_dynamic<concrete<T>, buffer_size>()},
        m_ptr{construct<concrete<T>>(m_array, fwd(args)...)} {}

  template <typename T>
  constexpr basic_erased(T x) noexcept
      : basic_erased{std::in_place_type<T>, static_cast<T &&>(x)} {}

  constexpr decltype(auto) invoke(auto method, auto &&...xs) const {
    return m_ptr->invoker(method, fwd(xs)...);
  }

  constexpr decltype(auto) invoke(auto method, auto &&...xs) {
    return m_ptr->invoker(method, fwd(xs)...);
  }

  constexpr basic_erased(basic_erased &&other) noexcept
    requires movable
      : m_dynamic(other.m_dynamic),
        m_ptr{other.m_ptr->move(m_dynamic, m_array)} {}

  constexpr basic_erased(const basic_erased &other)
    requires copyable
      : m_dynamic(other.m_dynamic),
        m_ptr{other.m_ptr->clone(m_dynamic, m_array)} {}

  constexpr basic_erased &operator=(basic_erased &&other) noexcept
    requires movable
  {
    destroy();
    m_dynamic = other.m_dynamic;
    m_ptr = other.m_ptr->move(m_dynamic, m_array);
    return *this;
  }

  constexpr basic_erased &operator=(const basic_erased &other)
    requires copyable
  {
    destroy();
    m_dynamic = other.m_dynamic;
    m_ptr = other.m_ptr->clone(m_dynamic, m_array);
    return *this;
  }
  constexpr void destroy() {
    if (m_dynamic)
      delete m_ptr;
    else
      m_ptr->~Base();
  }

  constexpr ~basic_erased() { destroy(); }
};

template <typename... Methods> using erased = basic_erased<32, Methods...>;
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
