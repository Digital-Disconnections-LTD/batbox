#include "runtime/AsyncLaunch.hpp"
#include "model/LiveStream.hpp"
#include <utility>

namespace batbox::model {

LiveStream::LiveStream(transport::Client& client, Json body, std::stop_token userSignal)
    : client_(client), body_(std::move(body)) {
  if (userSignal.stop_possible()) {
    userCancel_ = std::make_unique<std::stop_callback<std::function<void()>>>(
      userSignal, [this] { abort(); });
  }
  producer_ = ::batbox::runtime::contextThread([this] { produce(); });
}

LiveStream::~LiveStream() {
  abort();
  if (producer_.joinable()) producer_.join();
  userCancel_.reset();
}

void LiveStream::produce() {
  try {
    auto response = client_.request(body_, true,
      [this](const Json& event) {
        std::unique_lock lock(mutex_);
        queueChanged_.wait(lock, [this] { return events_.size() < 1 || aborted_; });
        if (aborted_) return;
        events_.push_back(event);
        queueChanged_.notify_all();
      }, cancel_.get_token(),
      [this](const transport::Response& response) {
        std::lock_guard lock(mutex_);
        response_ = response;
        if (response.status >= 200 && response.status < 300) {
          headersReady_ = true;
          ready_.notify_all();
        }
      });
    std::lock_guard lock(mutex_);
    response_ = std::move(response);
    done_ = true;
    ready_.notify_all();
    queueChanged_.notify_all();
  } catch (...) {
    std::lock_guard lock(mutex_);
    failure_ = std::current_exception();
    done_ = true;
    ready_.notify_all();
    queueChanged_.notify_all();
  }
}

transport::Response LiveStream::open() {
  std::unique_lock lock(mutex_);
  ready_.wait(lock, [this] { return headersReady_ || done_; });
  if (!headersReady_ && failure_) std::rethrow_exception(failure_);
  return response_;
}

std::optional<Json> LiveStream::next() {
  std::unique_lock lock(mutex_);
  queueChanged_.wait(lock, [this] { return !events_.empty() || done_; });
  if (!events_.empty()) {
    Json event = std::move(events_.front());
    events_.pop_front();
    queueChanged_.notify_all();
    return event;
  }
  if (failure_) std::rethrow_exception(failure_);
  return std::nullopt;
}

void LiveStream::abort() noexcept {
  {
    std::lock_guard lock(mutex_);
    aborted_ = true;
  }
  cancel_.request_stop();
  queueChanged_.notify_all();
  ready_.notify_all();
}

bool LiveStream::aborted() const {
  std::lock_guard lock(mutex_);
  return aborted_;
}

} // namespace batbox::model
