#include "test.h"
#include <hi/hi.h>

using namespace hi;

int main(int argc, char** argv) {
  // We're trying to create a situation where the runloop of a queue gets into a potential deadlock
  // state where:
  //
  // (1) There are no uv events and so the runloop falls into an idle state (execution suspended)
  //     which will block until either A) a call to async() enqueues one or more blocks to be run,
  //     or B) the last reference to the queue is released.
  //
  // (2) We release our reference to the queue, which should cause it to wake up from its suspended
  //     state, finalize the runloop and finally deallocate itself.
  //

  semaphore sem;
  volatile bool did_release_queue = false;

  {
    queue q = queue("1");
    #if HI_DEBUG
    debug_ref_on_dealloc(q, [&](ref_counted* self) {
      did_release_queue = true;
    });
    #endif
    q.async([=,&did_release_queue]{
      // Since we must retain a reference until _after_ state (1), we capture `q` in this block
      // and then through a block enqueued to the main queue.
      main_queue().async([=]{
        print("Arrived at state (1)");
        queue local_ref = q;
        // When this block returns, we will transition into state (2)
      });
      main_queue().async([&]{
        // When we arrive here, we should be in state (2)
        #if HI_DEBUG
        assert_true(did_release_queue);
        #endif
        print("Arrived at state (2)");
      });
      // unblock the main queue, causing the program to exit when the main queue is empty
      sem.signal();
      // When we return from this block, the runloop will transition into state (1).
    }).resume();
    // A new thread might be started, so at this point in this main thread, we can't be sure if the
    // queue's runloop has been entered yet. To know this, we enqueue a block and resume the queue.

    // Here, our main reference to the queue will be released, however note that we have a bound
    // reference to the queue in two blocks.
  }

  // Main queue barrier
  main_queue().async([=]{ sem.wait(); });

  return main_loop();
}
