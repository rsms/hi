#include "test.h"
#include <hi/hi.h>

using namespace hi;

int main(int argc, char** argv) {
  // No local reference to a queue which will cause the queue to immediately enter a
  // "deallocate when done" state, even before starting its runloop and flushing itself.
  // If this mechanic fails and the queued block with `sem.signal()` is not run, we will eventually
  // be terminated by SIGALARM.
  semaphore sem;
  alarm(1);

  queue("q").async([&] {
    print("signal");
    sem.signal();
  }).resume();

  main_queue().async([&]{
    print("wait");
    sem.wait();
  });

  return main_loop();
}
