// Copyright (C) myl7
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <mutex>
#include <condition_variable>
#include <functional>
#include <cassert>

namespace ejobdp {

struct DispatcherProps {
  /**
   * @brief If true, all workers will only do one iteration for the job and then exit
   */
  bool oneshot = false;
};

/**
 * @brief Provide API to launch many workers and dispatch workers to them in the environment without threading support,
 * e.g., enclave
 *
 * You should instantiate a Dispatcher object as a global variable so that it can be accessed by different (enclave)
 * threads
 */
class Dispatcher {
 public:
  /**
   * @brief Construct a new Dispatcher object
   *
   * @param worker_num Number of workers run by #run_worker
   * @param props See DispatcherProps
   */
  Dispatcher(int worker_num, DispatcherProps props = DispatcherProps()) : kWorkerNum(worker_num), props_(props) {}

  /**
   * @brief Run a worker to wait for a new iteration to do the job
   *
   * The method blocks. You should run it in a standalone (enclave) thread.
   *
   * @param worker_id Integer ID. Only passed to @p job and not used elsewhere. It may be used by Dispatcher to manage
   * worker information in the future.
   * @param job Job done by the worker after receiving a new iteration from the dispatcher. To pass more context into
   * it, you should use the mutex given by worker_mutex() to protect a global variable, save data into the global
   * variable, and then get the data from it with the the mutex given by worker_mutex() locked.
   */
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

  /**
   * @brief Dispatch a new iteration to all workers
   *
   * To wait until all workers have done the job in the iteration, see wait_dispatched_jobs()
   *
   * @param setup Done before sending the signal of the new iteration. Executed when the mutex given by worker_mutex()
   * is locked.
   */
  void dispatch_jobs(std::function<void()> setup) {
    {
      std::lock_guard lock(worker_mutex_);
      worker_ctx_.iter_id++;
      setup();
    }
    worker_cv_.notify_all();
  }

  /**
   * @brief Terminate all workers
   *
   * No need to and should not call wait_dispatched_jobs() later
   */
  void exit() {
    dispatch_jobs([this]() { worker_ctx_.exit = true; });
  }

  /**
   * @brief Block until all workers have done the job
   */
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

  /**
   * @brief Get the mutex protecting the state shared by all workers
   *
   * @return std::mutex& Reference to the mutex
   */
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
