#pragma once
#include "bats/BatMissionRuntime.hpp"
#include "bats/CompletionSignal.hpp"
#include <future>

namespace batbox::bats {
struct ObservedBackgroundSignal {
  std::shared_future<void> future;
  std::shared_ptr<CompletionSignal> completion;
};
class ForegroundSignals {
 public:
  ObservedBackgroundSignal createObserved(const std::string& missionId);
  std::shared_future<void> create(const std::string& missionId);
  void resolve(const std::string& missionId);
  void erase(const std::string& missionId);
 private:
  struct Resolver {
    std::shared_ptr<std::promise<void>> promise;
    std::shared_ptr<CompletionSignal> completion;
  };
  std::mutex mutex_;
  std::unordered_map<std::string,Resolver> resolvers_;
  std::vector<Resolver> abandoned_;
};
struct ForegroundServices {
  BatRegistrationServices* registration=nullptr;
  BatMissionRuntime* runtime=nullptr;
  ForegroundSignals* signals=nullptr;
  std::function<std::function<void()>(double delayMs,
    const std::function<void()>& callback)> scheduleAutoBackground;
};
struct RegisteredForegroundBat {
  std::string missionId;
  RegisteredLocalBat registered;
  std::shared_future<void> backgroundSignal;
  std::shared_ptr<CompletionSignal> backgroundCompletion;
  std::function<void()> cancelAutoBackground;
};
RegisteredForegroundBat registerBatForeground(
  const BatRegistrationParams& params,std::optional<double> autoBackgroundMs,
  ForegroundServices& services);
bool backgroundBatMission(const std::string& missionId,ForegroundServices& services);
void unregisterBatForeground(const std::string& missionId,ForegroundServices& services);
} // namespace batbox::bats
