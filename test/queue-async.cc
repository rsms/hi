#include "test.h"
#include <hi/hi.h>
#include <uv.h>

using namespace hi;

int main(int argc, char** argv) {
  queue q1 = queue("Q1");
  assert_not_null(q1);
  assert_not_null(q1->self);
  assert_eq_cstr(q1.label().c_str(), "Q1");
  alarm(1);

  bool q2_did_run = false;
  semaphore q2_sem;

  { // test is_current()
    queue q2 = queue("Q2");
    assert_eq_cstr(q2.label().c_str(), "Q2");
    q2.resume();
    q2.async([=,&q2_did_run]{
      assert_true(q2.is_current());
      assert_false(main_queue().is_current());
      assert_false(q1.is_current());
      q2_did_run = true;
      print("signal");
      q2_sem.signal();
    });
    // should live on past this scope even though `q` will disappear
  }

  int result[10];
  int result_len = 0;

  main_queue().async([&]{ result[result_len++] = 0; });
  main_queue().async([&]{ result[result_len++] = 1; });
  main_queue().async([&]{ result[result_len++] = 2; });
  main_queue().async([&]{
    assert_eq(result[0], 0);
    assert_eq(result[1], 1);
    assert_eq(result[2], 2);
    print("wait");
    q2_sem.wait();
    assert_true(q2_did_run);
  });
  main_queue().async([]{
    assert_true(main_queue().is_current());
  });

  return main_loop();
}
