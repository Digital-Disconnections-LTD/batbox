#pragma once
#include "terminal_reconciler/HostLeaves.hpp"
#include <optional>

namespace batbox::terminal_reconciler {
struct YogaCounters {double visited=0,measured=0,stashHits=0,live=0;};
struct CommitLifecycleImports {
  std::optional<std::string> commitLogPath;
  std::function<double()> performanceNow;
  std::function<void(const std::string&,const std::string&)> appendFileSync;
  std::function<YogaCounters()> getYogaCounters;
  std::function<bool()> isTestMode;
};
class CommitLifecycle {
public:
  CommitLifecycle(ProfileCounters& profile,CommitLifecycleImports imports);
  void prepareForCommit();
  void recordCreatedNode();
  void resetAfterCommit(terminal_dom::DOMElement& root);
private:
  double now()const;
  void append(const std::string& value)const;
  ProfileCounters& profile_;
  CommitLifecycleImports imports_;
  double commits_=0,lastLog_=0,lastCommitAt_=0,maxGapMs_=0,
    createCount_=0,prepareAt_=0;
};
} // namespace batbox::terminal_reconciler
