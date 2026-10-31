#pragma once

#include "transport/Client.hpp"
#include <condition_variable>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>

namespace batbox::model {

// Indigenous SDK-stream adapter. open() returns at successful response headers;
// next() exposes parsed events and subsequent body failures in arrival order.
class LiveStream {
 public:
  LiveStream(transport::Client& client, Json body, std::stop_token userSignal = {});
  ~LiveStream();
  LiveStream(const LiveStream&) = delete;
  LiveStream& operator=(const LiveStream&) = delete;
  transport::Response open();
  std::optional<Json> next();
  void abort() noexcept;
  bool aborted() const;
 private:
  void produce();
  transport::Client& client_;
  Json body_;
  std::stop_source cancel_;
  std::unique_ptr<std::stop_callback<std::function<void()>>> userCancel_;
  std::thread producer_;
  mutable std::mutex mutex_;
  std::condition_variable ready_;
  std::condition_variable queueChanged_;
  std::deque<Json> events_;
  transport::Response response_;
  std::exception_ptr failure_;
  bool headersReady_ = false;
  bool done_ = false;
  bool aborted_ = false;
};

} // namespace batbox::model
