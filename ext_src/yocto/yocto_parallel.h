//
// # Yocto/Parallel: Parallel utilities
//
// Yocto/Parallel is a collection of utilities helpful in implementing other
// Yocto/GL libraries. Yocto/Parallel is implemented in `yocto_parallel.h`.
//

//
// LICENSE:
//
// Copyright (c) 2016 -- 2022 Fabio Pellacini
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//

#ifndef _YOCTO_PARALLEL_H_
#define _YOCTO_PARALLEL_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include <atomic>
#include <deque>
#include <future>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

// -----------------------------------------------------------------------------
// USING DIRECTIVES
// -----------------------------------------------------------------------------
namespace yocto {

// using directives
using std::atomic;
using std::deque;
using std::future;
using std::mutex;
using std::vector;

}  // namespace yocto

// -----------------------------------------------------------------------------
// CONCURRENCY UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// a simple concurrent queue that locks at every call
template <typename T>
struct concurrent_queue {
  concurrent_queue()                              = default;
  concurrent_queue(const concurrent_queue& other) = delete;
  concurrent_queue& operator=(const concurrent_queue& other) = delete;

  bool empty();
  void clear();
  void push(const T& value);
  bool try_pop(T& value);

 private:
  std::mutex mutex;
  deque<T>   queue;
};

// Run a task asynchronously
template <typename Func, typename... Args>
inline auto run_async(Func&& func, Args&&... args);

// Check if an async task is ready
inline bool is_valid(const future<void>& result);
inline bool is_running(const future<void>& result);
inline bool is_ready(const future<void>& result);

// Simple parallel for used since our target platforms do not yet support
// parallel algorithms. `Func` takes the integer index.
template <typename T, typename Func>
inline void parallel_for(T num, Func&& func);
// Simple parallel for used since our target platforms do not yet support
// parallel algorithms. `Func` takes the two integer indices.
template <typename T, typename Func>
inline void parallel_for(T num1, T num2, Func&& func);
// Simple parallel for used since our target platforms do not yet support
// parallel algorithms. `Func` takes the integer index.
// Works on `batch` sized chunks.
template <typename T, typename Func>
inline void parallel_for_batch(T num, T batch, Func&& func);

// Simple parallel for used since our target platforms do not yet support
// parallel algorithms. `Func` takes a reference to a `T`.
template <typename T, typename Func>
inline void parallel_foreach(vector<T>& values, Func&& func);
template <typename T, typename Func>
inline void parallel_foreach(const vector<T>& values, Func&& func);

// Simple parallel for used since our target platforms do not yet support
// parallel algorithms. `Func` takes the integer index, a string ref,
// and returns a bool. Handles errors explicitly.
template <typename T, typename Func>
inline bool parallel_for(T num, string& error, Func&& func);

// Simple parallel for used since our target platforms do not yet support
// parallel algorithms. `Func` takes a reference to a `T`, a string ref,
// and returns a bool. Handles errors explicitly.
template <typename T, typename Func>
inline bool parallel_foreach(vector<T>& values, string& error, Func&& func);
template <typename T, typename Func>
inline bool parallel_foreach(
    const vector<T>& values, string& error, Func&& func);

}  // namespace yocto

// -----------------------------------------------------------------------------
//
//
// IMPLEMENTATION
//
//
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// CONCURRENCY UTILITIES
// -----------------------------------------------------------------------------
namespace yocto {

// a simple concurrent queue that locks at every call
template <typename T>
bool concurrent_queue<T>::empty() {
  std::lock_guard<std::mutex> lock(mutex);
  return queue.empty();
}
template <typename T>
void concurrent_queue<T>::clear() {
  std::lock_guard<std::mutex> lock(mutex);
  queue.clear();
}
template <typename T>
void concurrent_queue<T>::push(const T& value) {
  std::lock_guard<std::mutex> lock(mutex);
  queue.push_back(value);
}
template <typename T>
bool concurrent_queue<T>::try_pop(T& value) {
  std::lock_guard<std::mutex> lock(mutex);
  if (queue.empty()) return false;
  value = queue.front();
  queue.pop_front();
  return true;
}

// Run a task asynchronously
template <typename Func, typename... Args>
inline auto run_async(Func&& func, Args&&... args) {
  return std::async(std::launch::async, std::forward<Func>(func),
      std::forward<Args>(args)...);
}
// Check if an async task is ready
inline bool is_valid(const future<void>& result) { return result.valid(); }
inline bool is_running(const future<void>& result) {
  return result.valid() && result.wait_for(std::chrono::microseconds(0)) !=
                               std::future_status::ready;
}
inline bool is_ready(const future<void>& result) {
  return result.valid() && result.wait_for(std::chrono::microseconds(0)) ==
                               std::future_status::ready;
}

// Simple parallel for used since our target platforms do not yet support
// parallel algorithms. `Func` takes the integer index.
template <typename T, typename Func>
inline void parallel_for(T num, Func&& func) {
  auto         futures  = vector<future<void>>{};
  auto         nthreads = std::thread::hardware_concurrency();
  atomic<T>    next_idx(0);
  atomic<bool> has_error(false);
  for (auto thread_id = 0; thread_id < (int)nthreads; thread_id++) {
    futures.emplace_back(
        std::async(std::launch::async, [&func, &next_idx, &has_error, num]() {
          try {
            while (true) {
              auto idx = next_idx.fetch_add(1);
              if (idx >= num) break;
              if (has_error) break;
              func(idx);
            }
          } catch (...) {
            has_error = true;
            throw;
          }
        }));
  }
  for (auto& f : futures) f.get();
}

// Simple parallel for used since our target platforms do not yet support
// parallel algorithms. `Func` takes the two integer indices.
template <typename T, typename Func>
inline void parallel_for(T num1, T num2, Func&& func) {
  auto         futures  = vector<future<void>>{};
  auto         nthreads = std::thread::hardware_concurrency();
  atomic<T>    next_idx(0);
  atomic<bool> has_error(false);
  for (auto thread_id = 0; thread_id < (int)nthreads; thread_id++) {
    futures.emplace_back(std::async(
        std::launch::async, [&func, &next_idx, &has_error, num1, num2]() {
          try {
            while (true) {
              auto j = next_idx.fetch_add(1);
              if (j >= num2) break;
              if (has_error) break;
              for (auto i = (T)0; i < num1; i++) func(i, j);
            }
          } catch (...) {
            has_error = true;
            throw;
          }
        }));
  }
  for (auto& f : futures) f.get();
}

// Simple parallel for used since our target platforms do not yet support
// parallel algorithms. `Func` takes the integer index.
template <typename T, typename Func>
inline void parallel_for_batch(T num, T batch, Func&& func) {
  auto         futures  = vector<future<void>>{};
  auto         nthreads = std::thread::hardware_concurrency();
  atomic<T>    next_idx(0);
  atomic<bool> has_error(false);
  for (auto thread_id = 0; thread_id < (int)nthreads; thread_id++) {
    futures.emplace_back(std::async(
        std::launch::async, [&func, &next_idx, &has_error, num, batch]() {
          try {
            while (true) {
              auto start = next_idx.fetch_add(batch);
              if (start >= num) break;
              if (has_error) break;
              auto end = std::min(num, start + batch);
              for (auto i = (T)start; i < end; i++) func(i);
            }
          } catch (...) {
            has_error = true;
            throw;
          }
        }));
  }
  for (auto& f : futures) f.get();
}

// Simple parallel for used since our target platforms do not yet support
// parallel algorithms. `Func` takes a reference to a `T`.
template <typename T, typename Func>
inline void parallel_foreach(vector<T>& values, Func&& func) {
  parallel_for(
      values.size(), [&func, &values](size_t idx) { func(values[idx]); });
}
template <typename T, typename Func>
inline void parallel_foreach(const vector<T>& values, Func&& func) {
  parallel_for(
      values.size(), [&func, &values](size_t idx) { func(values[idx]); });
}

// Simple parallel for used since our target platforms do not yet support
// parallel algorithms. `Func` takes the integer index.
template <typename T, typename Func>
inline bool parallel_for(T num, string& error, Func&& func) {
  auto         futures  = vector<future<void>>{};
  auto         nthreads = std::thread::hardware_concurrency();
  atomic<T>    next_idx(0);
  atomic<bool> has_error(false);
  mutex        error_mutex;
  for (auto thread_id = 0; thread_id < (int)nthreads; thread_id++) {
    futures.emplace_back(std::async(std::launch::async,
        [&func, &next_idx, &has_error, &error_mutex, &error, num]() {
          auto this_error = string{};
          while (true) {
            if (has_error) break;
            auto idx = next_idx.fetch_add(1);
            if (idx >= num) break;
            if (!func(idx, this_error)) {
              has_error = true;
              auto _    = std::lock_guard{error_mutex};
              error     = this_error;
              break;
            }
          }
        }));
  }
  for (auto& f : futures) f.get();
  return !(bool)has_error;
}

// Simple parallel for used since our target platforms do not yet support
// parallel algorithms. `Func` takes a reference to a `T`.
template <typename T, typename Func>
inline bool parallel_foreach(vector<T>& values, string& error, Func&& func) {
  return parallel_for(
      values.size(), error, [&func, &values](size_t idx, string& error) {
        return func(values[idx], error);
      });
}
template <typename T, typename Func>
inline bool parallel_foreach(
    const vector<T>& values, string& error, Func&& func) {
  return parallel_for(
      values.size(), error, [&func, &values](size_t idx, string& error) {
        return func(values[idx], error);
      });
}

}  // namespace yocto

#endif
