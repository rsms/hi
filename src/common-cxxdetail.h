#ifndef SYNCBOX_COMMON_CXXDETAIL_H
#define SYNCBOX_COMMON_CXXDETAIL_H

#ifndef _HI_INDIRECT_INCLUDE_
#error "Please #include <hi/common.h> instead of this file directly."
#endif

#include <type_traits>

#define HI_CXX_DISALLOW_COPY(T) \
  T(const T& other) = delete; \
  T& operator=(const T& other) = delete

namespace hi { namespace detail {

// Support for C++11 enum class comparison
template<typename T> using _UT = typename std::underlying_type<T>::type;
template<typename T> constexpr _UT<T> underlying_type(T t) { return _UT<T>(t); }
template<typename T> struct TruthValue {
  T t;
  constexpr TruthValue(T t): t(t) { }
  constexpr operator T() const { return t; }
  constexpr explicit operator bool() const { return underlying_type(t); }
};

}} // namespace hi::detail

// This would allow _any_ enum class to be bitwise operated
//template <typename T>
//constexpr hi::detail::TruthValue<T>
//operator&(T lhs, T rhs) {
//  return T(hi::detail::underlying_type(lhs) & hi::detail::underlying_type(rhs));
//}

// This allows explicit enum classes to be bitwise operated
#define HI_CXX_DECL_ENUM_BITWISE_OPS(T) \
  namespace { \
    HI_UNUSED \
    constexpr ::hi::detail::TruthValue<T> \
    operator&(T lhs, T rhs) { \
      return T(::hi::detail::underlying_type(lhs) \
             & ::hi::detail::underlying_type(rhs)); \
    } \
  }

#endif // SYNCBOX_COMMON_CXXDETAIL_H
