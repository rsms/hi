#include "test.h"
#include <hi/hi.h>

using namespace hi;

int main(int argc, char** argv) {
  // main queue thread_id should be configured already
  assert_true(main_queue().is_current());

  // should be same objects and not null
  queue& q0 = main_queue();
  queue& q1 = main_queue();
  assert_eq(q0->self, q1->self);
  assert_not_null(q0->self);
  assert_eq(&q0, &q1);
  return 0;
}
