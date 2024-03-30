// Copyright (C) myl7
// SPDX-License-Identifier: Apache-2.0

#include <mutex>
#include <thread>
#include <vector>
#include <cassert>
#include <functional>
#include "dispatch.hpp"

constexpr int kJobNum = 4;
ejobdp::Dispatcher gDispatcher(kJobNum - 1);

struct WorkerArgs {
  int i;
};
WorkerArgs gWorkerArgs;

int main() {
  std::vector<int> res;
  std::mutex res_mutex;

  std::vector<std::thread> workers;
  std::function<void(int)> dispatcher_job;
  for (int i = 0; i < kJobNum; i++) {
    auto job = [&res, &res_mutex](int id) {
      int i;
      {
        std::lock_guard lock(gDispatcher.worker_mutex());
        i = gWorkerArgs.i;
      }

      {
        std::lock_guard lock(res_mutex);
        res.push_back(i * 10 + id);
      }
    };
    if (i == 0) {
      dispatcher_job = job;
    } else {
      workers.emplace_back([job = std::move(job), i]() { gDispatcher.run_worker(i, job); });
    }
  }

  gDispatcher.dispatch_jobs([]() { gWorkerArgs.i = 1; });
  dispatcher_job(0);
  gDispatcher.wait_dispatched_jobs();

  assert(res.size() == kJobNum);
  for (const auto &elem : res) {
    assert(int(elem / 10) == 1);
  }
  res.clear();

  gDispatcher.dispatch_jobs([]() { gWorkerArgs.i = 2; });
  dispatcher_job(0);
  gDispatcher.wait_dispatched_jobs();

  assert(res.size() == kJobNum);
  for (const auto &elem : res) {
    assert(int(elem / 10) == 2);
  }
  res.clear();

  gDispatcher.exit();
  for (auto &worker : workers) {
    worker.join();
  }
  return 0;
}
