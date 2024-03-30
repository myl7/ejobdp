#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>
#include <cassert>

namespace ejobdp {

struct WorkerCtx {
  size_t kWorkerNum;
  size_t done_num;
  size_t iter_id = 0;
  bool exit = false;
};

extern std::mutex gWorkerMutex;
extern std::condition_variable gWorkerCv;
extern WorkerCtx gWorkerCtx;

static void dispatch_jobs(std::function<void()> setup) {
  {
    std::lock_guard lock(gWorkerMutex);
    gWorkerCtx.iter_id++;
    setup();
  }
  gWorkerCv.notify_all();
}

static void wait_dispatched_jobs() {
  {
    std::unique_lock lock(gWorkerMutex);
    gWorkerCv.wait(lock, []() {
      assert((void("invalid done_num: should <= kWorkerNum"), gWorkerCtx.done_num <= gWorkerCtx.kWorkerNum));
      return gWorkerCtx.done_num == gWorkerCtx.kWorkerNum;
    });
  }

  {
    std::lock_guard lock(gWorkerMutex);
    gWorkerCtx.done_num = 0;
  }
}

static void do_job(std::function<void()> job, bool oneshot = false) {
  for (size_t i = 1; true; i++) {
    bool exit = false;
    {
      std::unique_lock lock(gWorkerMutex);
      gWorkerCv.wait(lock, [i, &exit]() {
        if (gWorkerCtx.exit) {
          exit = true;
          return true;
        }
        assert((void("invalid iter_id: should increase step-by-step"), gWorkerCtx.iter_id <= i));
        return gWorkerCtx.iter_id == i;
      });
    }

    if (exit) {
      return;
    }
    job();

    bool all_done = false;
    {
      std::lock_guard lock(gWorkerMutex);
      gWorkerCtx.done_num++;
      if (gWorkerCtx.done_num == gWorkerCtx.kWorkerNum) {
        all_done = true;
      }
    }
    if (all_done) {
      gWorkerCv.notify_all();
    }

    if (oneshot) {
      return;
    }
  }
}

}  // namespace ejobdp
