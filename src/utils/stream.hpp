#pragma once
#include "runtime/JsonValue.hpp"
#include <deque>
#include <exception>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <vector>

namespace batbox {
template<class T> struct IteratorResult {
  bool done;
  std::optional<T> value;
};
class StreamError : public std::runtime_error {
public:
  Json value;
  explicit StreamError(Json error) : std::runtime_error(value::stringify(error)), value(std::move(error)) {}
};

// Each Stream instance supports one iterator.
// return_ is named for the C++ reserved keyword.
template<class T> class Stream {
  using Result = IteratorResult<T>;
  std::deque<T> queue;
  std::shared_ptr<std::promise<Result>> readResolve;
  bool isDone = false;
  Json hasError;
  bool started = false;
  std::function<void()> returned;
  std::mutex mutex;
  // An overwritten JS resolver leaves its promise pending. Retain the C++
  // promise too, rather than accidentally completing it with broken_promise.
  std::vector<std::shared_ptr<std::promise<Result>>> overwritten;
  static std::future<Result> resolved(Result value) {
    std::promise<Result> promise;
    auto future = promise.get_future();
    promise.set_value(std::move(value));
    return future;
  }
public:
  explicit Stream(std::function<void()> callback = {}) : returned(std::move(callback)) {}
  Stream& iterate() {
    std::lock_guard lock(mutex);
    if (started) throw std::runtime_error("Stream can only be iterated once");
    started = true;
    return *this;
  }
  std::future<Result> next() {
    std::lock_guard lock(mutex);
    if (!queue.empty()) {
      auto value = std::move(queue.front()); queue.pop_front();
      return resolved({false, std::move(value)});
    }
    if (isDone) return resolved({true, std::nullopt});
    if (value::truthy(hasError)) {
      std::promise<Result> rejected;
      auto future = rejected.get_future();
      rejected.set_exception(std::make_exception_ptr(StreamError(hasError)));
      return future;
    }
    if (readResolve) overwritten.push_back(readResolve);
    readResolve = std::make_shared<std::promise<Result>>();
    return readResolve->get_future();
  }
  void enqueue(T value) {
    std::lock_guard lock(mutex);
    if (readResolve) {
      auto resolve = std::move(readResolve);
      resolve->set_value({false, std::move(value)});
    } else queue.push_back(std::move(value));
  }
  void done() {
    std::lock_guard lock(mutex);
    isDone = true;
    if (readResolve) {
      auto resolve = std::move(readResolve);
      resolve->set_value({true, std::nullopt});
    }
  }
  void error(Json errorValue) {
    std::lock_guard lock(mutex);
    hasError = std::move(errorValue);
    if (readResolve) {
      auto reject = std::move(readResolve);
      reject->set_exception(std::make_exception_ptr(StreamError(hasError)));
    }
  }
  std::future<Result> return_() {
    {
      std::lock_guard lock(mutex);
      isDone = true;
    }
    if (returned) returned();
    return resolved({true, std::nullopt});
  }
};
} // namespace batbox
