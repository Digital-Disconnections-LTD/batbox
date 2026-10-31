#pragma once
#include "bats/BatMissionRegistration.hpp"
#include <mutex>
#include <unordered_map>

namespace batbox::bats {
// Indigenous sidecars for AbortController and cleanup roster closures, which
// cannot live in the serializable AppState JSON object.
class BatMissionRuntime {
 public:
  void remember(const std::string& missionId,const RegisteredLocalBat& mission);
  void bind(BatMissionStateServices& services);
  std::shared_ptr<std::stop_source> abortOwner(const std::string& missionId) const;
 private:
  struct Entry {
    std::shared_ptr<std::stop_source> abortOwner;
    std::shared_ptr<void> parentAbortLink;
    std::function<void()> unregisterCleanup;
  };
  mutable std::mutex mutex_;
  std::unordered_map<std::string,Entry> entries_;
};
} // namespace batbox::bats
