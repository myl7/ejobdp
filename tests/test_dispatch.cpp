#include <thread>
#include <vector>
#include <cassert>
#include "dispatch.hpp"

namespace ejobdp {

std::mutex gWorkerMutex;
std::condition_variable gWorkerCv;
WorkerCtx gWorkerCtx;

}  // namespace ejobdp

struct WorkerArgs {
  int i;
};

WorkerArgs gWorkerArgs;

int main() {
  std::mutex res_mutex;
  std::vector<int> res;

  constexpr size_t kJobs = 2;
  {
    std::lock_guard lock(ejobdp::gWorkerMutex);
    ejobdp::gWorkerCtx.kWorkerNum = kJobs;
  }

  std::vector<std::thread> workers;
  for (size_t i = 0; i < kJobs; i++) {
    auto job = [id = i, &res, &res_mutex]() {
      decltype(gWorkerArgs.i) i;
      {
        std::lock_guard lock(ejobdp::gWorkerMutex);
        i = gWorkerArgs.i;
      }

      {
        std::lock_guard lock(res_mutex);
        res.push_back(i * 10 + id);
      }
    };
    workers.emplace_back([job = std::move(job)]() { ejobdp::do_job(job); });
  }

  ejobdp::dispatch_jobs([]() { gWorkerArgs.i = 1; });
  ejobdp::wait_dispatched_jobs();

  assert(res.size() == kJobs);
  for (const auto &elem : res) {
    assert(int(elem / 10) == 1);
  }
  res.clear();

  ejobdp::dispatch_jobs([]() { gWorkerArgs.i = 2; });
  ejobdp::wait_dispatched_jobs();

  assert(res.size() == kJobs);
  for (const auto &elem : res) {
    assert(int(elem / 10) == 2);
  }

  ejobdp::dispatch_jobs([]() { ejobdp::gWorkerCtx.exit = true; });
  for (auto &worker : workers) {
    worker.join();
  }

  return 0;
}
