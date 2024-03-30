#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>
#include <cassert>

namespace ejobdp {

struct DispatcherProps {
  bool oneshot = false;
};

class Dispatcher {
 public:
  Dispatcher(int worker_num, DispatcherProps props = DispatcherProps()) : kWorkerNum(worker_num), props_(props) {}

  void run_worker(int worker_id, std::function<void(int)> job) {
    for (size_t i = 1; true; i++) {
      bool exit = false;
      {
        std::unique_lock lock(worker_mutex_);
        worker_cv_.wait(lock, [this, i, &exit]() {
          if (worker_ctx_.exit) {
            exit = true;
            return true;
          }
          assert((void("invalid iter_id: should increase step-by-step"), worker_ctx_.iter_id <= i));
          return worker_ctx_.iter_id == i;
        });
      }
      if (exit) {
        return;
      }

      job(worker_id);

      bool all_done = false;
      {
        std::lock_guard lock(worker_mutex_);
        worker_ctx_.done_num++;
        if (worker_ctx_.done_num == kWorkerNum) {
          all_done = true;
        }
      }
      if (all_done) {
        worker_cv_.notify_all();
      }

      if (props_.oneshot) {
        return;
      }
    }
  }

  void dispatch_jobs(std::function<void()> setup) {
    {
      std::lock_guard lock(worker_mutex_);
      worker_ctx_.iter_id++;
      setup();
    }
    worker_cv_.notify_all();
  }

  void exit() {
    dispatch_jobs([this]() { worker_ctx_.exit = true; });
  }

  void wait_dispatched_jobs() {
    {
      std::unique_lock lock(worker_mutex_);
      worker_cv_.wait(lock, [this]() {
        assert((void("invalid done_num: should <= kWorkerNum"), worker_ctx_.done_num <= kWorkerNum));
        return worker_ctx_.done_num == kWorkerNum;
      });
    }

    {
      std::lock_guard lock(worker_mutex_);
      worker_ctx_.done_num = 0;
    }
  }

  std::mutex &worker_mutex() {
    return worker_mutex_;
  }

 private:
  int kWorkerNum;
  DispatcherProps props_;

  struct WorkerCtx {
    int done_num = 0;
    size_t iter_id = 0;
    bool exit = false;
  };
  WorkerCtx worker_ctx_;
  std::mutex worker_mutex_;
  std::condition_variable worker_cv_;
};

}  // namespace ejobdp
