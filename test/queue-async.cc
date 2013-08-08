#include "test.h"
#include <hi/hi.h>

using namespace hi;

int main(int argc, char** argv) {
  queue q1 = queue("Q1");
  assert_not_null(q1);
  assert_not_null(q1->self);
  assert_eq_cstr(q1.label().c_str(), "Q1");

  bool q2_did_run = false;

  { // test is_current()
    queue q = queue("Q2");
    assert_eq_cstr(q.label().c_str(), "Q2");
    q.resume();
    q.async([=,&q2_did_run]{
      assert_true(q.is_current());
      assert_false(main_queue().is_current());
      assert_false(q1.is_current());
      q2_did_run = true;
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
    hi::sleep(0.05); // make sure "Q2" is empty before we execute
    assert_true(q2_did_run);
  });

  return main_loop();
}
