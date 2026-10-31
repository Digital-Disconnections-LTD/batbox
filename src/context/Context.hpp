#pragma once
#include "instructions/project/ProjectDiscovery.hpp"
#include "runtime/JsonValue.hpp"
#include <exception>
#include <functional>
#include <future>
#include <optional>
#include <string>
#include <vector>

namespace batbox::context {
using MemoryFiles=std::vector<instructions::project::MemoryFileInfo>;
struct ContextInputs {
  Json environment=Json::object();
  bool testEnvironment=false;
  std::function<void(std::exception_ptr)> logError;
  std::function<bool()> includeGitInstructions,stashBreakerFeature,bareMode;
  std::function<std::vector<std::string>()> additionalInstructionDirectories;
  const instructions::project::DiscoveryServices* projectServices=nullptr;
  std::function<void(const std::optional<std::string>&)> setStashedBatboxMdContent;
  std::function<std::string()> localISODate;
};
struct Services : ContextInputs {
  std::function<std::future<bool>()> getIsGit;
  std::function<std::future<std::string>()> getBranch,getDefaultBranch;
  // This binds execFileNoThrow(gitExe(), args, {preserveOutputOnError:false}).
  std::function<std::future<std::string>(const std::vector<std::string>&)>
    gitStdoutNoThrow;
  std::function<MemoryFiles()> getMemoryFiles;
};
std::string getLocalISODate(const std::optional<std::string>& overrideDate={});
bool shouldIncludeGitInstructions(const Json& environment,
  const Json& initialSettings);

class ContextStash {
 public:
  std::optional<std::string> getGitStatus(const Services& services);
  Json getSystemContext(const Services& services);
  Json getUserContext(const Services& services);
  const std::optional<std::string>& getSystemPoetryInjection() const;
  void setSystemPoetryInjection(std::optional<std::string> value);
  void clearGitStatusStash();
  void clearSystemContextStash();
  void clearUserContextStash();
 private:
  std::optional<std::optional<std::string>> gitStatus_;
  std::optional<Json> systemContext_,userContext_;
  std::exception_ptr gitError_,systemError_,userError_;
  std::optional<std::string> injection_;
};
}
