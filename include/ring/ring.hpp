#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <utility>

// #if defined(__cpp_lib_hardware_interference_size)
// constexpr std::size_t cache_line_size =
//     std::hardware_destructive_interference_size;
// #else
// constexpr std::size_t cache_line_size = 64;
// #endif

constexpr size_t cache_line_size = 64;

namespace shyamnatesan {

template <typename T, typename Allocator = std::allocator<T>> class Ring {
public:
  using AllocTraits = std::allocator_traits<Allocator>;
  using ptr = typename AllocTraits::pointer;

  explicit Ring(size_t capacity, const Allocator &alloc)
      : alloc_(alloc), capacity_(check_capacity(capacity)) {
    buffer_ = AllocTraits::allocate(alloc_, capacity_);
  }

  ~Ring() {
    if (buffer_ == nullptr) {
      return;
    }

    destroy_live();
    AllocTraits::deallocate(alloc_, buffer_, capacity_);
  }

  Ring(const Ring &) = delete;
  Ring &operator=(const Ring &) = delete;

  Ring(Ring &&) = delete;
  Ring &operator=(Ring &&) = delete;

private:
  [[no_unique_address]] Allocator alloc_{};

  size_t capacity_{};
  ptr buffer_{nullptr};

  static_assert(std::atomic<size_t>::is_always_lock_free,
                "atomic<size_t> is not always lock-free");

  alignas(cache_line_size) std::atomic<size_t> write_idx_{};
  size_t read_cached_{};

  alignas(cache_line_size) std::atomic<size_t> read_idx_{};
  size_t write_cached_{};

public:
  // Here, in-order to distinguish between a full and empty queue,
  // we maintain 2 indices, read/write indices, which is only incremented
  // monotonically, and only wrapped around for indexing into the buffer.
  // This approach avoids having to keep track of a placeholder slot in the
  // buffer, which is another common approach for distringuishing between full
  // and empty queue.

  bool TryPush(const T &value) {
    auto wi = write_idx_.load(std::memory_order_relaxed);

    if (wi - read_cached_ == capacity_) [[unlikely]] {
      read_cached_ = read_idx_.load(std::memory_order_acquire);
      if (wi - read_cached_ == capacity_) [[unlikely]] {
        return false;
      }
    }

    AllocTraits::construct(alloc_, slot(wi), value);
    write_idx_.store(wi + 1, std::memory_order_release);
    return true;
  }

  bool TryPop(T &value) {
    auto ri = read_idx_.load(std::memory_order_relaxed);

    if (ri == write_cached_) [[unlikely]] {
      write_cached_ = write_idx_.load(std::memory_order_acquire);
      if (ri == write_cached_) [[unlikely]] {
        return false;
      }
    }

    auto *s = slot(ri);
    value = std::move(*s);
    AllocTraits::destroy(alloc_, s);
    read_idx_.store(ri + 1, std::memory_order_release);
    return true;
  }

private:
  void destroy_live() noexcept {
    // destruct all live objects
    auto ri = read_idx_.load(std::memory_order_relaxed);
    auto wi = write_idx_.load(std::memory_order_relaxed);
    for (; ri != wi; ++ri) {
      AllocTraits::destroy(alloc_, slot(ri));
    }
  }

  T *slot(size_t i) noexcept { return buffer_ + (i & (capacity_ - 1)); }

  static size_t check_capacity(size_t capacity) {
    // check if capacity is a power of 2, return capacity if it is, else throw
    if ((capacity & (capacity - 1)) != 0) {
      throw std::invalid_argument("capacity must be a power of 2");
    }
    return capacity;
  }
};

} // namespace shyamnatesan
