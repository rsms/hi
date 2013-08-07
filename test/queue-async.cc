#include "test.h"
#include <hi/hi.h>

using namespace hi;

int main(int argc, char** argv) {
  // ref<Queue> q1 = Queue::create("Q1");
  // q1->resume();
  // q1->async([=]{
  //   printf("[q1] q1.is_current() -> %d\n", q1->is_current());
  //   main_queue().async([=]{
  //     printf("[main] q1.is_current() -> %d\n", q1->is_current());
  //   });
  // });

  // {
  //   ref<Queue> q2 = Queue::create("Q2");
  //   q2->resume();
  //   q2->async([=]{
  //     printf("[q2] q2.is_current() -> %d\n", q2->is_current());
  //   });
  // }

  main_queue()->async([]{
    std::cout << "[main] Hello\n";
    // main_exit();
  });

  print("[main] main_loop()\n");
  return main_loop();
}

