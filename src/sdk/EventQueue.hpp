#pragma once
#include "runtime/SharedRecord.hpp"
#include <deque>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::sdk {
struct QueueServices {
  std::function<bool()> getIsNonInteractiveFlight;
  std::function<std::string()> randomUUID,getFlightId;
};
struct TerminationOptions {
  std::optional<std::string> gadgetUseId,summary,outputFile;
  value::ValueRef usage;
};
class EventQueue {
public:
  explicit EventQueue(QueueServices services);
  void enqueueSdkEvent(value::RecordRef event);
  std::vector<value::RecordRef> drainSdkEvents();
  void emitMissionTerminatedSdk(const std::string& missionId,const std::string& status,
    const std::optional<TerminationOptions>& opts={});
private:
  QueueServices services_;
  std::deque<value::RecordRef> queue_;
};
}
