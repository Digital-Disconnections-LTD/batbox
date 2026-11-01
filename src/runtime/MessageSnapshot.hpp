#pragma once
#include "runtime/JsonValue.hpp"
#include "runtime/MessageMutation.hpp"
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

namespace batbox::runtime {
// A message is shared: the stream yields it before message_delta
// changes usage and stop_reason. The indigenous flight writer may drain on another
// thread, so both the reducer mutation and the delayed writer snapshot use this
// lock. This guards the Json *object*, without replacing its shared identity.
namespace detail {
inline std::mutex messageRosterMutex;
inline std::unordered_map<const Json*,std::weak_ptr<std::mutex>> messageRoster;
inline std::shared_ptr<std::mutex> mutexFor(const std::shared_ptr<Json>& message) {
  if (!message) throw std::invalid_argument("Cannot lock a null message reference");
  std::lock_guard rosterLock(messageRosterMutex);
  const auto key=message.get();
  if (auto existing=messageRoster[key].lock()) return existing;
  auto mutex=std::make_shared<std::mutex>();
  messageRoster[key]=mutex;
  if (messageRoster.size()>4096) {
    for (auto it=messageRoster.begin();it!=messageRoster.end();) {
      if (it->second.expired()) it=messageRoster.erase(it);
      else ++it;
    }
  }
  return mutex;
}
} // namespace detail

class MessageLock {
  std::shared_ptr<Json> message_;
  std::shared_ptr<std::mutex> mutex_;
  std::unique_lock<std::mutex> lock_;
  bool mutated_=false;
public:
  explicit MessageLock(std::shared_ptr<Json> message):
    message_(std::move(message)),mutex_(detail::mutexFor(message_)),lock_(*mutex_){}
  MessageLock(const MessageLock&)=delete;
  MessageLock& operator=(const MessageLock&)=delete;
  // Mutable access publishes the edit to memoized views once the lock ends.
  ~MessageLock(){if(mutated_)noteMessageMutated(message_.get());}
  Json& value(){mutated_=true;return *message_;}
  const Json& value()const{return *message_;}
};

inline Json snapshotMessage(const std::shared_ptr<Json>& message) {
  const MessageLock lock(message);
  return lock.value();
}
} // namespace batbox::runtime
