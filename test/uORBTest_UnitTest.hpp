/****************************************************************************
 *
 *   Copyright (c) 2012-2015 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#pragma once

#include <uorb/base/abs_time.h>
#include <uorb/topics/orb_test.h>
#include <uorb/topics/orb_test_large.h>
#include <uorb/topics/orb_test_medium.h>
#include <uorb/uorb.h>

#include "posix_task.h"

typedef const orb_metadata *orb_id_t;

#include <unistd.h>

#include <cerrno>

namespace uORBTest {
class UnitTest;
}

class uORBTest::UnitTest {
 public:
  // Singleton pattern
  static uORBTest::UnitTest &instance();
  ~UnitTest() = default;
  int test();
  template <typename S>
  int latency_test(orb_id_t T, bool print);

  // Disallow copy
  UnitTest(const uORBTest::UnitTest & /*unused*/) = delete;

 private:
  UnitTest() : pub_sub_test_passed(false), pub_sub_test_print(false) {}

  static int pub_sub_test_threadEntry(int argc, char **argv);
  int pub_sub_latency_main();

  static int pub_test_multi2_entry(int argc, char *argv[]);
  int pub_test_multi2_main();

  volatile bool _thread_should_exit{};

  bool pub_sub_test_passed;
  bool pub_sub_test_print;
  int pub_sub_test_res = true;

  orb_advert_t _pfd[4]{};  ///< used for test_multi and test_multi_reversed

  static int test_single();

  /* These 3 depend on each other and must be called in this order */
  int test_multi();
  int test_multi_reversed();
  int test_unadvertise();

  int test_multi2();

  /* queuing tests */
  static int test_queue();
  static int pub_test_queue_entry(int argc, char *argv[]);
  int pub_test_queue_main();
  int test_queue_poll_notify();
  volatile int _num_messages_sent = 0;

  static int test_fail(const char *fmt, ...);
  static int test_note(const char *fmt, ...);
};

template <typename S>
int uORBTest::UnitTest::latency_test(orb_id_t T, bool print) {
  test_note("---------------- LATENCY TEST ------------------");
  S t{};
  t.val = 308;
  t.timestamp = orb_absolute_time();

  orb_advert_t pfd0 = orb_advertise(T, &t);

  if (pfd0 == nullptr) {
    return test_fail("orb_advertise failed (%i)", errno);
  }

  char *const args[1] = {nullptr};

  pub_sub_test_print = print;
  pub_sub_test_passed = false;

  /* test pub / sub latency */

  // Can't pass a pointer in args, must be a null terminated
  // array of strings because the strings are copied to
  // prevent access if the caller data goes out of scope
  auto pub_sub_task = task_spawn_cmd(
      "uorb_latency", 3000,
      (thread_entry_t)&uORBTest::UnitTest::pub_sub_test_threadEntry, args);

  /* give the test task some data */
  while (!pub_sub_test_passed) {
    ++t.val;
    t.timestamp = orb_absolute_time();

    if (!orb_publish(T, pfd0, &t)) {
      return test_fail("mult. pub0 timing fail");
    }

    /* simulate >800 Hz system operation */
    usleep(1000);
  }

  if (pub_sub_task < 0) {
    return test_fail("%s: failed launching task", __FUNCTION__);
  }

  orb_unadvertise(&pfd0);

  return pub_sub_test_res;
}
