#pragma once

#include "runtime/JsonValue.hpp"
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::model_stash {

struct Services {
  std::string excludedModelFamily;
  std::function<double()> nowMs;
  std::function<double(const std::string&)> parseTimestampMs;
  std::function<std::optional<std::string>(const std::string&,const std::string&)>
    writeStashBreakDiff;
  std::function<void(const std::string&,const std::string&)> logForDebugging;
  std::function<void(const std::string&)> logError;
};

struct PendingChanges {
  Json flags=Json::object();
  int addedGadgetCount=0,removedGadgetCount=0,systemCharDelta=0;
  std::vector<std::string> addedGadgets,removedGadgets,changedGadgetSchemas;
  std::string previousModel,newModel,prevGlobalStashStrategy,newGlobalStashStrategy;
  std::vector<std::string> addedBetas,removedBetas;
  std::string prevEffortValue,newEffortValue;
  Json prevSystemForDiff=Json::array(),prevGadgetsForDiff=Json::array();
  bool hasPreviousDiffableContent=false;
  bool flag(const char* name) const;
};

struct PreviousState {
  std::int32_t systemHash=0,gadgetsHash=0,stashControlHash=0,extraBodyHash=0;
  std::vector<std::string> gadgetNames,betas;
  std::map<std::string,std::int32_t> perGadgetHashes;
  int systemCharCount=0,callCount=0;
  std::string model,globalStashStrategy,effortValue;
  Json systemForDiff=Json::array(),gadgetsForDiff=Json::array();
  bool fastMode=false,autoModeActive=false,isUsingOverage=false,
    stashedMCEnabled=false,stashDeletionsPending=false;
  std::optional<double> prevStashReadTokens;
  std::optional<PendingChanges> pendingChanges;
};

struct PendingCheck {
  std::shared_ptr<PreviousState> state;
  std::string querySource,reason,previousContent,currentContent;
  double previousRead=0,stashReadTokens=0,stashCreationTokens=0;
};

// Per-flight local stash diagnostic state. No remote analytics leave this object.
class Detector {
public:
  explicit Detector(Services services);
  void recordPoetryState(const Json& snapshot);
  void checkResponseForStashBreak(const std::string& querySource,
      double stashReadTokens,double stashCreationTokens,const Json& messages,
      const std::optional<std::string>& batId={},
      const std::optional<std::string>& requestId={});
  // Split the check around the awaited local diff write. The first
  // method runs on the query owner; finish runs on that owner after I/O.
  std::optional<PendingCheck> beginResponseCheck(const std::string& querySource,
      double stashReadTokens,double stashCreationTokens,const Json& messages,
      const std::optional<std::string>& batId={},
      const std::optional<std::string>& requestId={});
  void finishResponseCheck(PendingCheck pending,
      const std::optional<std::string>& diffPath);
  void notifyStashDeletion(const std::string& querySource,
      const std::optional<std::string>& batId={});
  void notifyCompaction(const std::string& querySource,
      const std::optional<std::string>& batId={});
  void cleanupBatTracking(const std::string& batId);
  void resetPoetryStashBreakDetection();
  std::optional<PreviousState> stateFor(const std::string& key) const;
  std::vector<std::string> trackingKeys() const;

private:
  Services services_;
  std::map<std::string,std::shared_ptr<PreviousState>> states_;
  std::list<std::string> insertionOrder_;
  std::optional<std::string> trackingKey(const std::string& querySource,
      const std::optional<std::string>& batId) const;
  void erase(const std::string& key);
};

std::string buildDiffableContent(const Json& system,const Json& gadgets,
    const std::string& model);

} // namespace batbox::model_stash
