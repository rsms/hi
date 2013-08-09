#include "test.h"
#include <hi/hi.h>

using namespace hi;

int main(int argc, char** argv) {
  semaphore sem;
  volatile int count = 0;
  alarm(1);

  queue("1").async([&]{
    hi::sleep(0.05);
    hi_atomic_add32(&count, 1);
    print("signal");
    sem.signal();
  }).resume();

  queue("2").async([&]{
    hi_atomic_add32(&count, 1);
    print("signal");
    sem.signal();
  }).resume();

  main_queue().async([&]{
    hi::sleep(0.01);
    print("wait");
    sem.wait();
    print("wait");
    sem.wait();
    assert_eq(count, 2);
  });

  return main_loop();
}
