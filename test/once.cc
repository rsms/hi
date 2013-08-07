#include "test.h"
#include <hi/hi.h>

using namespace hi;

once_flag once1;
once_flag once2;
int count1 = 0;
volatile long count2 = 0;
semaphore sem1;

void f1() {
  ++count1;
}

int main(int argc, char** argv) {
  once(once1, f1);
  once(once1, []{ ++count1; });
  assert_eq(count1, 1);

  // async([]{ print("from the background woooo... spooky"); });
  // async(print, "from the background woooo... spooky");

  queue q1 = queue_::create("Q1");
  queue q2 = queue_::create("Q2");
  q1->async([]{
    once(once2, [](int a, int b){ hi_atomic_add_fetch(&count2, a + b); }, 1, 2);
    sem1.signal();
  });
  q2->async([]{
    once(once2, [](int b){ hi_atomic_add_fetch(&count2, b); }, 3);
    sem1.signal();
  });
  q1->resume();
  q2->resume();
  sem1.wait();
  sem1.wait();
  assert_eq(count2, 3);

  return 0;
}

