#include "test.h"
#include <hi/hi.h>
#include <hi/rusage.h>

using namespace hi;

int main(int argc, char** argv) {
  hi::rusage::sample ru;
  semaphore sem1;
  int N = 8;
  volatile int count = 0;
  for (int i = 0; i != N; ++i) {
    hi::async([&]{ hi_atomic_add32(&count, 1); sem1.signal(); });
  }
  for (int i = 0; i != N; ++i) {
    sem1.wait();
  }
  assert_eq(count, N);
  #if !HI_TEST_SUIT_RUNNING
  ru.delta_dumpn(N, "");
  #endif
  return 0;
}
