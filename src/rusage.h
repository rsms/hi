#ifndef _HI_RUSAGE_H_
#define _HI_RUSAGE_H_

#include <hi/common.h>
#include <sys/time.h>
#include <sys/resource.h>

namespace hi {
namespace rusage {

struct sample {
  sample(const char* label=NULL) { _label = label ? strdup(label) : 0; (*this)(); }
  ~sample();
  void operator()() {
    gettimeofday(&rtime, 0);
    getrusage(RUSAGE_SELF, &r);
  }
  uint64_t real_usec() const;
  uint64_t user_usec() const;
  uint64_t system_usec() const;
  uint64_t delta_real_usec() const;
  uint64_t delta_user_usec() const;
  uint64_t delta_system_usec() const;
  void delta_dump(const char* label=0, const char* leading=0) const;
private:
  // bool           _delta = false;
  struct rusage  r;
  struct timeval rtime;
  char* _label;
};


inline static uint64_t HI_UNUSED to_usec(const struct timeval& tv) {
  return ((uint64_t)tv.tv_sec * 1000000ULL) + tv.tv_usec;
}
inline static HI_UNUSED double to_ms(const struct timeval& tv) {
  return ((double)tv.tv_sec * 1000.0) + ((double)tv.tv_usec / 1000.0);
}

inline static void HI_UNUSED
timeval_delta(const struct timeval& a,
              const struct timeval& b,
              struct timeval& d) {
  uint64_t d_usec = to_usec(b) - to_usec(a);
  d.tv_sec = d_usec / 1000000ULL;
  d.tv_usec = d_usec - (d.tv_sec * 1000000ULL);
}

inline uint64_t sample::real_usec() const   { return to_usec(rtime); }
inline uint64_t sample::user_usec() const   { return to_usec(r.ru_utime); }
inline uint64_t sample::system_usec() const { return to_usec(r.ru_stime); }

inline uint64_t sample::delta_real_usec() const {
  struct timeval tv;
  gettimeofday(&tv, 0);
  return to_usec(tv) - to_usec(rtime);
}
inline uint64_t sample::delta_user_usec() const {
  struct rusage r2;
  getrusage(RUSAGE_SELF, &r2);
  return to_usec(r2.ru_utime) - to_usec(r.ru_utime);
}
inline uint64_t sample::delta_system_usec() const {
  struct rusage r2;
  getrusage(RUSAGE_SELF, &r2);
  return to_usec(r2.ru_stime) - to_usec(r.ru_stime);
}

inline sample::~sample() {
  if (_label != 0) {
    delta_dump(_label);
    free(_label);
  }
}

inline static void dump_sample(
  const char* leader, const struct timeval& rtime, const struct rusage& r, bool is_delta)
{
  // From http://man7.org/tlpi/code/online/dist/procres/print_rusage.c.html
  const char *ldr = (leader == nullptr) ? "" : leader;

  fprintf(stderr,
    "%sReal time:             % 15.0f µs\n", ldr, (double)to_usec(rtime));
  fprintf(stderr,
    "%sUser CPU time:         % 15.0f µs\n", ldr, (double)to_usec(r.ru_utime));
  fprintf(stderr,
    "%sSystem CPU time:       % 15.0f µs\n", ldr, (double)to_usec(r.ru_stime));
  fprintf(stderr,
    "%sMemory resident usage: % 15ld B\n", ldr, r.ru_maxrss);
  
  // #if HI_RUSAGE_SAMPLE_DETAILED
  fprintf(stderr,
    "%sIntegral shared memory:  %ld\n", ldr, r.ru_ixrss);
  fprintf(stderr,
    "%sIntegral unshared data:  %ld\n", ldr, r.ru_idrss);
  fprintf(stderr,
    "%sIntegral unshared stack: %ld\n", ldr, r.ru_isrss);
  // #endif
  
  fprintf(stderr,
    "%sMemory pages:            reclaims: %ld, faults: %ld\n", ldr, r.ru_minflt, r.ru_majflt);
  fprintf(stderr,
    "%sSwaps:                   %ld\n", ldr, r.ru_nswap);
  
  #if HI_RUSAGE_SAMPLE_DETAILED
  fprintf(stderr,
    "%sBlock I/Os:              in: %ld, out: %ld\n",
    ldr, r.ru_inblock, r.ru_oublock);
  #endif

  fprintf(stderr,
    "%sSignals received:        %ld\n", ldr, r.ru_nsignals);
  fprintf(stderr,
    "%sIPC messages:            sent: %ld, received: %ld\n",
    ldr, r.ru_msgsnd, r.ru_msgrcv);
  fprintf(stderr,
    "%sContext switches:        voluntary: %ld, involuntary: %ld\n",
    ldr, r.ru_nvcsw, r.ru_nivcsw);
}

inline void sample::delta_dump(const char* label, const char* leading) const {
  struct rusage r2, rD;
  struct timeval rtime2, rtimeD;
  getrusage(RUSAGE_SELF, &r2);
  gettimeofday(&rtime2, 0);

  const char* line_leader = leading == 0 ? "" : leading;
  if (label != 0) {
    fprintf(stderr, "%s\n", label);
    if (leading == 0) line_leader = "  ";
  }
  // dump_sample("  ", rtime, r, false);
  // fprintf(stderr, "\n");
  // dump_sample("  ", rtime2, r2, false);
  // fprintf(stderr, "\n");
  
  timeval_delta(rtime, rtime2, rtimeD);
  timeval_delta(r.ru_utime, r2.ru_utime, rD.ru_utime);
  timeval_delta(r.ru_stime, r2.ru_stime, rD.ru_stime);
  rD.ru_maxrss =    r2.ru_maxrss   - r.ru_maxrss;
  rD.ru_ixrss =     r2.ru_ixrss    - r.ru_ixrss;
  rD.ru_idrss =     r2.ru_idrss    - r.ru_idrss;
  rD.ru_isrss =     r2.ru_isrss    - r.ru_isrss;
  rD.ru_minflt =    r2.ru_minflt   - r.ru_minflt;
  rD.ru_majflt =    r2.ru_majflt   - r.ru_majflt;
  rD.ru_nswap =     r2.ru_nswap    - r.ru_nswap;
  rD.ru_inblock =   r2.ru_inblock  - r.ru_inblock;
  rD.ru_oublock =   r2.ru_oublock  - r.ru_oublock;
  rD.ru_msgsnd =    r2.ru_msgsnd   - r.ru_msgsnd;
  rD.ru_msgrcv =    r2.ru_msgrcv   - r.ru_msgrcv;
  rD.ru_nsignals =  r2.ru_nsignals - r.ru_nsignals;
  rD.ru_nvcsw =     r2.ru_nvcsw    - r.ru_nvcsw;
  rD.ru_nivcsw =    r2.ru_nivcsw   - r.ru_nivcsw;
  dump_sample(line_leader, rtimeD, rD, true);
}

// inline static void HI_UNUSED
// SResUsagePrintSummary(const SResUsage *start,
//                       const SResUsage *end,
//                       const char* opname,
//                       size_t opcount,
//                       size_t thread_count) {

// inline void sample::dump_summary(const char* leader) const {
//   const char* _opname = (opname == 0) ? "operation" : opname;
//   SResUsage ru_delta;
//   SResUsageDelta(start, end, &ru_delta);
//   SResUsagePrint("", &ru_delta, true);
//   double u_nsperop =
//     ((double)to_usec(&ru_delta.r.ru_utime) / opcount) * 1000.0;
//   double r_nsperop =
//     ((double)to_usec(&ru_delta.rtime) / opcount) * 1000.0;
//   print("Performance: %.0f ns per %s (CPU user time)", u_nsperop, _opname);
//   print("             %.0f ns per %s (real time)", r_nsperop, _opname);
//   if (thread_count > 1) {
//     print("Overhead:    %.0f ns in total",
//       (r_nsperop * thread_count) - u_nsperop );
//   }
//   print("%s total: %zu", _opname, opcount);
// }

}} // namespace

#endif  // _HI_RUSAGE_H_
