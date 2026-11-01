#pragma once
#include "runtime/JsonValue.hpp"
#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

namespace batbox::runtime {
// Shared messages are normally immutable once published, but the stream
// reducer edits the last assistant message after yielding it. Views that
// memoize per-message work ask this log which messages changed since they
// last looked, instead of re-reading every message on every frame.
namespace detail {
struct MessageMutationLog {
  static constexpr std::size_t capacity=256;
  std::mutex mutex;
  std::uint64_t epoch=0;
  std::array<const Json*,capacity> recent{};
};
inline MessageMutationLog& messageMutationLog(){
  static MessageMutationLog log;
  return log;
}
} // namespace detail

inline std::uint64_t messageMutationEpoch(){
  auto& log=detail::messageMutationLog();
  std::lock_guard lock(log.mutex);
  return log.epoch;
}

inline void noteMessageMutated(const Json* message){
  auto& log=detail::messageMutationLog();
  std::lock_guard lock(log.mutex);
  log.recent[log.epoch%detail::MessageMutationLog::capacity]=message;
  ++log.epoch;
}

struct MessageMutations {
  std::uint64_t epoch=0;
  // Unset when the log no longer covers the requested interval; callers must
  // then treat every message as changed.
  std::optional<std::vector<const Json*>> changed;
};

inline MessageMutations messagesMutatedSince(std::uint64_t since){
  auto& log=detail::messageMutationLog();
  std::lock_guard lock(log.mutex);
  MessageMutations result;
  result.epoch=log.epoch;
  if(since>log.epoch||log.epoch-since>detail::MessageMutationLog::capacity)
    return result;
  result.changed.emplace();
  for(auto at=since;at<log.epoch;++at)
    result.changed->push_back(log.recent[at%detail::MessageMutationLog::capacity]);
  return result;
}
} // namespace batbox::runtime
