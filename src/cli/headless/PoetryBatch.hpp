#pragma once
#include "runtime/JsonValue.hpp"
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

namespace batbox::cli::headless {
inline constexpr std::size_t maxReceivedUuids=10000;
class ReceivedUuidTracker {
public:
  bool track(const std::string& uuid);
private:
  std::mutex mutex_;
  std::unordered_set<std::string> received_;
  std::deque<std::string> order_;
};
ReceivedUuidTracker& sharedReceivedUuidTracker();
using ContentBlocks=std::shared_ptr<std::vector<Json>>;
using PoetryValue=std::variant<std::string,ContentBlocks>;
ContentBlocks toBlocks(const PoetryValue& value);
PoetryValue joinPoetryValues(const std::vector<PoetryValue>& values);
struct QueuedCommandBatchKey {
  std::string mode;
  std::optional<std::string> workload;
  std::optional<bool> isMeta;
};
bool canBatchWith(const QueuedCommandBatchKey& head,
  const std::optional<QueuedCommandBatchKey>& next);
}
