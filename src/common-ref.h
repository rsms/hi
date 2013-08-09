#ifndef _HI_REF_H_
#define _HI_REF_H_

#ifndef _HI_INDIRECT_INCLUDE_
#error "Please #include <hi/common.h> instead of this file directly."
#endif

namespace hi {

// Reference-counted POD-style objects
struct ref_counted {
  volatile uint32_t __refcount = 1;
#if HI_DEBUG
  std::function<void()> __debug_on_dealloc;
#endif
};

// Example:
// lolcat.h:
//   struct lolcat { HI_REF_MIXIN(lolcat)
//     lolcat(bool happy);
//     void set_happy(bool);
//     bool happy() const;
//   };
// lolcat.cc:
//   struct lolcat::S : ref_counted {
//     bool happy;
//   };
//   void lolcat::dealloc(S* p) { delete self; }
//   lolcat::lolcat(bool happy) : lolcat(new S) { self->happy = happy; }
//   void lolcat::set_happy(bool b) { self->happy = b; }
//   bool lolcat::happy() const { return self->happy; }
// main.cc:
//   int main() {
//     lolcat cat(true);
//     cat->set_happy(false);
//     std::cout << cat->happy() << std::endl; // 0
//     return 0; // cat 
//   }
//
// Must implement `static void dealloc(S* p)`.
// 
#define HI_REF_MIXIN(T) \
public: \
  struct S; friend S; S* self = nullptr; \
  \
  static void dealloc(S* p); \
  static void __retain(S* p) { \
    if (p != nullptr) { hi_atomic_add32((int32_t*)&((::hi::ref_counted*)p)->__refcount, 1); } \
  } \
  static void __release(S* p) { \
    if (p != nullptr \
        && (hi_atomic_sub_fetch(&((::hi::ref_counted*)p)->__refcount, 1) == 0)) { \
      _HI_DEBUG_REF_DEALLOC(p) \
      dealloc(p); \
    } \
  } \
  /*T() : self(0) {}*/ \
  T(std::nullptr_t) : self(nullptr) {} \
  explicit T(S* p, bool addRef=false) : self(p) { \
    /*printf("** new ctor (p=%p, addRef=%s)\n", self, addRef ? "true":"false");*/ \
    if (addRef) { __retain(self); } \
  } \
  T(T const& rhs) : self(rhs.self) { \
    /*printf("Copy ctor (p=%p)\n", self);*/ \
    __retain(self); \
  } \
  T(const T* rhs) : self(rhs->self) { \
    /*printf("Copy ctor (p=%p)\n", self);*/ \
    __retain(self); \
  } \
  T(T&& rhs) { \
    /*printf("Move ctor\n");*/ \
    self = std::move(rhs.self); rhs.self = 0; \
    /* self = hi_atomic_swap(&rhs.self, 0); */ \
  } \
  ~T() { \
    __release(self); \
  } \
  inline T& operator=(T&& rhs) { \
    /*printf("Move op\n");*/ \
    if (self != rhs.self) { \
      __release(self); \
      self = rhs.self; \
    } \
    /* self = hi_atomic_swap(&rhs.self, 0); */ \
    rhs.self = 0; \
    return *this; \
  } \
  inline T& operator=(const T& rhs) { \
    /*printf("Copy op (p=%p, rhs.p=%p)\n", self, rhs.self);*/ \
    return reset(rhs.self); } \
  inline T& operator=(S* rhs) { return reset(rhs); } \
  inline T& operator=(const S* rhs) { return reset(rhs); } \
  inline T& operator=(std::nullptr_t) { return reset(nullptr); } \
  bool is_null() const { return (self == nullptr); } \
  T& reset(const S* p = nullptr) const { \
    S* old = self; \
    __retain((const_cast<T*>(this)->self = const_cast<S*>(p))); \
    __release(old); \
    return *const_cast<T*>(this); \
  } \
  T* operator->() { return this; } \
  T* operator->() const { return const_cast<T*>(this); } \
  bool operator==(const T& other) const { return self == other.self; } \
  bool operator!=(const T& other) const { return self != other.self; } \
  bool operator==(std::nullptr_t) const { return self == nullptr; } \
  bool operator!=(std::nullptr_t) const { return self != nullptr; } \
  operator bool() const { return self != nullptr; } \
  /*const T* operator->() const { return this; }*/
  /*S& operator*() const { return *((S*)self); }*/
// end of HI_REF_MIXIN


// template<typename T> inline void retain_noreturn(T* p) {
//   //printf("** INCR %p\n", p);
//   hi_atomic_add32((int32_t*)&p->__refcount, 1);
//   assert(p->__refcount != UINT32_MAX); // or overflow
// }
// template<typename T> inline T* retain(T* p) { retain_noreturn(p); return p; }
// template<typename T> inline bool release(T* p) {
//   //printf("** DECR %p\n", p);
//   return (hi_atomic_sub_fetch(&p->__refcount, 1) == 0);
// }

#if HI_DEBUG
  // Used for unit tests
  template <typename T>
  void debug_ref_on_dealloc(T& obj, std::function<void(ref_counted*)> b) {
    ref_counted* self = (ref_counted*)obj.self;
    self->__debug_on_dealloc = [=]{ b(self); };
  }
  #define _HI_DEBUG_REF_DEALLOC(p) do { \
    if ((bool)((::hi::ref_counted*)(p))->__debug_on_dealloc) { \
      ((::hi::ref_counted*)(p))->__debug_on_dealloc(); \
    } \
  } while(0);
#else
  #define _HI_DEBUG_REF_DEALLOC(p)
#endif

// template <typename T> using ref = ::std::shared_ptr<T>;

} // namespace

#endif  // _HI_REF_H_
