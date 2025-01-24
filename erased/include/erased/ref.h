#pragma once

#include "utils/utils.h"
#include <memory>
#include <tuple>
#include <type_traits>
#include <typeinfo>

namespace erased {

template <typename... Methods> class ref;

template <typename T> struct is_ref : std::false_type {};

template <typename... Methods>
struct is_ref<ref<Methods...>> : std::true_type {};

template <typename T> constexpr auto is_ref_v = is_ref<T>::value;

template <typename T>
concept ref_concept = is_ref_v<std::decay_t<T>>;

template <typename... Methods> class ref : public Methods... {
  static constexpr auto all_const =
      (details::method_to_trait_t<Methods>::is_const && ...);

  using vtable = details::vtable<Methods...>;

public:
  template <typename T>
  constexpr ref(T &object) noexcept
      : m_ptr{std::addressof(object)},
        m_vtable{vtable::template construct_for<T>()} {}

  template <typename Method, typename... Args>
  constexpr decltype(auto) invoke(Method, Args &&...args) const {
    return m_vtable->template get<Method>()(m_ptr, static_cast<Args>(args)...);
  }

  template <typename T, typename... M>
  friend constexpr bool is(const ref<M...> &object);

  template <typename T, ref_concept Erased>
  friend constexpr auto *any_cast(Erased *object);

  template <typename T, ref_concept Erased>
  friend constexpr auto &any_cast(Erased &&object);

private:
  details::fast_conditional<all_const>::template apply<const void *, void *>
      m_ptr;
  const vtable *m_vtable;
};

template <typename T, typename... Methods>
constexpr bool is(const ref<Methods...> &object) {
  return object.m_vtable ==
         ref<Methods...>::vtable::template construct_for<T>();
}

template <typename T, ref_concept Erased>
constexpr auto *any_cast(Erased *object) {
  if constexpr (std::is_same_v<const void *, decltype(object->m_ptr)> ||
                std::is_const_v<std::remove_reference_t<Erased>>) {
    if (is<T>(*object)) {
      return static_cast<const T *>(object->m_ptr);
    }
    return static_cast<const T *>(nullptr);
  } else {
    if (is<T>(*object))
      return static_cast<T *>(object->m_ptr);
    return static_cast<T *>(nullptr);
  }
}

template <typename T, ref_concept Erased>
constexpr auto &any_cast(Erased &&object) {
  if constexpr (std::is_same_v<const void *, decltype(object.m_ptr)> ||
                std::is_const_v<std::remove_reference_t<Erased>>) {
    if (is<T>(object))
      return *static_cast<const T *>(object.m_ptr);
  } else {
    if (is<T>(object))
      return *static_cast<T *>(object.m_ptr);
  }
  throw std::bad_cast{};
}

} // namespace erased
