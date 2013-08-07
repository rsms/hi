#ifndef _HI_ATOMIC_H_
#define _HI_ATOMIC_H_
/*

T hi_atomic_swap(T *ptr, T value)
  Atomically swap integers or pointers in memory. Note that this is more than just CAS.
  E.g: int old_value = hi_atomic_swap(&value, new_value);

void hi_atomic_add32(int32_t* operand, int32_t delta)
  Increment a 32-bit integer `operand` by `delta`. There's no return value.

T hi_atomic_add_fetch(T* operand, T delta)
  Add `delta` to `operand` and return the resulting value of `operand`

T hi_atomic_sub_fetch(T* operand, T delta)
  Subtract `delta` from `operand` and return the resulting value of `operand`

bool hi_atomic_cas_bool(T* ptr, T oldval, T newval)
  If the current value of *ptr is oldval, then write newval into *ptr. Returns true if the
  operation is successful and newval was written.

T hi_atomic_cas(T* ptr, T oldval, T newval)
  If the current value of *ptr is oldval, then write newval into *ptr. Returns the contents of
  *ptr before the operation.

-----------------------------------------------------------------------------*/

#ifndef _HI_INDIRECT_INCLUDE_
#error "Please #include <hi/common.h> instead of this file directly."
#endif

#define _HI_ATOMIC_HAS_SYNC_BUILTINS defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 4))

// T hi_atomic_swap(T *ptr, T value)
#if HI_WITHOUT_SMP
  #define hi_atomic_swap(ptr, value)  \
    ({ __typeof__ (value) oldval = *(ptr); \
       *(ptr) = (value); \
       oldval; })
#elif defined(__clang__)
  // This is more efficient than the below fallback
  #define hi_atomic_swap __sync_swap
#elif _HI_ATOMIC_HAS_SYNC_BUILTINS
  static inline void* HI_UNUSED _hi_atomic_swap(void* volatile* ptr, void* value) {
    void* oldval;
    do {
      oldval = *ptr;
    } while (__sync_val_compare_and_swap(ptr, oldval, value) != oldval);
    return oldval;
  }
  #define hi_atomic_swap(ptr, value) \
    _hi_atomic_swap((void* volatile*)(ptr), (void*)(value))
#else
  #error "Unsupported compiler: Missing support for atomic operations"
#endif

// void hi_atomic_add32(T* operand, T delta)
#if HI_WITHOUT_SMP
  #define hi_atomic_add32(operand, delta) (*(operand) += (delta))
#elif HI_TARGET_ARCH_X64 || HI_TARGET_ARCH_X86
  inline static void HI_UNUSED hi_atomic_add32(int32_t* operand, int32_t delta) {
    // From http://www.memoryhole.net/kyle/2007/05/atomic_incrementing.html
    __asm__ __volatile__ (
      "lock xaddl %1, %0\n" // add delta to operand
      : // no output
      : "m" (*operand), "r" (delta)
    );
  }
#elif _HI_ATOMIC_HAS_SYNC_BUILTINS
  #define hi_atomic_add32 __sync_add_and_fetch
#else
  #error "Unsupported compiler: Missing support for atomic operations"
#endif

// T hi_atomic_sub_fetch(T* operand, T delta)
#if HI_WITHOUT_SMP
  #define hi_atomic_sub_fetch(operand, delta) (*(operand) -= (delta))
#elif _HI_ATOMIC_HAS_SYNC_BUILTINS
  #define hi_atomic_sub_fetch __sync_sub_and_fetch
#else
  #error "Unsupported compiler: Missing support for atomic operations"
#endif

// T hi_atomic_add_fetch(T* operand, T delta)
#if HI_WITHOUT_SMP
  #define hi_atomic_add_fetch(operand, delta) (*(operand) += (delta))
#elif _HI_ATOMIC_HAS_SYNC_BUILTINS
  #define hi_atomic_add_fetch __sync_add_and_fetch
#else
  #error "Unsupported compiler: Missing support for atomic operations"
#endif


// bool hi_atomic_cas_bool(T* ptr, T oldval, T newval)
#if HI_WITHOUT_SMP
  #define hi_atomic_cas_bool(ptr, oldval, newval)  \
    (*(ptr) == (oldval) && (*(ptr) = (newval)))
#elif _HI_ATOMIC_HAS_SYNC_BUILTINS
  #define hi_atomic_cas_bool(ptr, oldval, newval) \
    __sync_bool_compare_and_swap((ptr), (oldval), (newval))
#else
  #error "Unsupported compiler: Missing support for atomic operations"
#endif


// T hi_atomic_cas(T* ptr, T oldval, T newval)
#if HI_WITHOUT_SMP
  #define hi_atomic_cas(ptr, oldval, newval)  \
    ({ __typeof__(oldval) prevv = *(ptr); \
       if (*(ptr) == (oldval)) { *(ptr) = (newval); } \
       prevv; })
#elif _HI_ATOMIC_HAS_SYNC_BUILTINS
  #define hi_atomic_cas(ptr, oldval, newval) \
    __sync_val_compare_and_swap((ptr), (oldval), (newval))
#else
  #error "Unsupported compiler: Missing support for atomic operations"
#endif


// Spinlock
#ifdef __cplusplus
namespace hi {
typedef volatile int32_t Spinlock;

#define SB_SPINLOCK_INIT 0  // usage: Spinlock lock = SB_SPINLOCK_INIT;

bool spinlock_try_lock(Spinlock& lock);
void spinlock_lock(Spinlock& lock);
void spinlock_unlock(Spinlock& lock);

bool spinlock_try_lock(Spinlock* lock);
void spinlock_lock(Spinlock* lock);
void spinlock_unlock(Spinlock* lock);

struct ScopedSpinlock {
  ScopedSpinlock(Spinlock& lock) : _lock(lock) { spinlock_lock(_lock); }
  ~ScopedSpinlock() { spinlock_unlock(_lock); }
private:
  Spinlock& _lock;
};

inline bool HI_UNUSED spinlock_try_lock(Spinlock& lock) {
  return hi_atomic_cas_bool(&lock, (int32_t)0, (int32_t)1); }
inline void HI_UNUSED spinlock_lock(Spinlock& lock) {
  while (!spinlock_try_lock(lock)); }
inline void HI_UNUSED spinlock_unlock(Spinlock& lock) {
  lock = 0; }

inline bool HI_UNUSED spinlock_try_lock(Spinlock* lock) {
  return hi_atomic_cas_bool(lock, (int32_t)0, (int32_t)1); }
inline void HI_UNUSED spinlock_lock(Spinlock* lock) {
  while (!spinlock_try_lock(lock)); }
inline void HI_UNUSED spinlock_unlock(Spinlock* lock) {
  *lock = 0; }

} // namespace
#endif // __cplusplus


#endif // _HI_ATOMIC_H_
