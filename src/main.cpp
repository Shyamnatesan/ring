

#include "ring/ring.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <thread>
#include <utility>

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace {

inline bool pin_this_thread(int core) {
#ifdef __linux__
  cpu_set_t set;
  CPU_ZERO(&set);
  CPU_SET(core, &set);
  return pthread_setaffinity_np(pthread_self(), sizeof(set), &set) == 0;
#else
  std::fprintf(stderr, "pin_this_thread: affinity not supported on this OS\n");
  return false;
#endif
}

inline void name_this_thread(const char *name) {
#ifdef __linux__
  pthread_setname_np(pthread_self(), name);
#else
#endif
}

template <typename Fn>
std::thread spawn_thread(int core, const char *name, Fn &&fn) {
  std::atomic<bool> ready{false};
  std::atomic<bool> failed{false};

  std::thread t([core, name, fn = std::forward<Fn>(fn), &ready, &failed] {
    name_this_thread(name);
    if (core >= 0 && !pin_this_thread(core)) {
      std::fprintf(stderr, "failed to pin %s to core %d\n", name, core);
      failed.store(true, std::memory_order_release);
      return;
    }
    ready.store(true, std::memory_order_release);
    fn();
  });

  while (!ready.load(std::memory_order_acquire) &&
         !failed.load(std::memory_order_acquire)) {
    std::this_thread::yield();
  }

  if (failed.load(std::memory_order_acquire)) {
    t.join();
    std::fprintf(stderr, "thread %s did not start\n", name);
  }

  return t;
}

} // namespace

void Test() {
  auto N = 10000;
  auto capacity = 1024;

  shyamnatesan::Ring<int> ring(capacity, {});

  std::thread producer([&] {
    for (int i = 0; i < N; ++i) {
      while (!ring.TryPush(i)) {
      }
    }
  });

  for (auto expected{0}; expected < N; ++expected) {
    int got{-1};

    while (!ring.TryPop(got)) {
    }

    if (got != expected) {
      std::fprintf(stderr, "mismatch at %d: got %d\n", expected, got);
      producer.join();
      return;
    }
  }

  producer.join();
  std::puts("ok");
}

double run_once(size_t capacity, int n, int prod_cpu, int cons_cpu) {
  shyamnatesan::Ring<int> ring(capacity, {});
  std::atomic<bool> go{false};

  auto producer = spawn_thread(prod_cpu, "producer", [&] {
    while (!go.load(std::memory_order_acquire)) {
    }

    for (int i{}; i < n; ++i) {
      while (!ring.TryPush(i)) {
      }
    }
  });

  if (cons_cpu >= 0) {
    pin_this_thread(cons_cpu);
  }

  name_this_thread("consumer");

  go.store(true, std::memory_order_release);

  auto start = std::chrono::steady_clock::now();
  for (int expected{}; expected < n; ++expected) {
    int got{-1};
    while (!ring.TryPop(got)) {
    }
    if (got != expected) {
      std::fprintf(stderr, "mismatch at %d: got %d\n", expected, got);
      std::abort();
    }
  }

  auto stop = std::chrono::steady_clock::now();
  producer.join();

  return n / std::chrono::duration<double>(stop - start).count();
}

void Bench(int prod_cpu, int cons_cpu) {
  constexpr std::size_t capacity = 1024;
  constexpr int n = 10'000'000;

  run_once(capacity, n, prod_cpu, cons_cpu); // warmup, discarded

  constexpr int runs = 5;
  std::vector<double> rates;
  rates.reserve(runs);
  for (int i = 0; i < runs; ++i)
    rates.push_back(run_once(capacity, n, prod_cpu, cons_cpu));
  std::sort(rates.begin(), rates.end());

  double median = rates[runs / 2];
  std::printf("capacity %zu, transfers %d, payload int, cores %d/%d\n",
              capacity, n, prod_cpu, cons_cpu);
  std::printf("median %.3f M transfers/sec, %.2f ns/transfer\n", median / 1e6,
              1e9 / median);
  std::printf("runs:");
  for (double r : rates)
    std::printf(" %.3f", r / 1e6);
  std::printf(" M/s\n");
}

int main(int argc, char **argv) {
  // Test
  Test();

  // Benchmark
  int prod_cpu = argc == 3 ? std::atoi(argv[1]) : -1;
  int cons_cpu = argc == 3 ? std::atoi(argv[2]) : -1;
  Bench(prod_cpu, cons_cpu);
}
