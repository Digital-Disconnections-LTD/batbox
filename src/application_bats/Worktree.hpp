#pragma once
#include "application/Config.hpp"
#include "bats/BatGadgetCall.hpp"
#include "event_loop/Loop.hpp"
#include "process/ExecFileNoThrow.hpp"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace batbox::application_hooks {class Provider;}
namespace batbox::application_bats {
struct WorktreeSettings {
  std::vector<std::string> sparsePaths;
  std::vector<std::string> symlinkDirectories;
};
WorktreeSettings parseWorktreeSettings(const Json& settings);
struct WorktreeInputs {
  event_loop::Loop& owner;
  application::Config config;
  std::shared_ptr<application_hooks::Provider> hooks;
  std::function<Json()> getSettings;
  std::function<void(const std::string&,const Json&)> enqueueBatMetadata;
  std::function<void(const std::string&)> diagnostic;
  explicit WorktreeInputs(event_loop::Loop& loop):owner(loop){}
};

// Indigenous provider for BatGadget worktree creation and cleanup.
// Git processes run on indigenous workers while their Promise continuations stay
// on the owner loop; BatGadget launch itself runs on its gadget worker.
class WorktreeProvider final {
public:
  explicit WorktreeProvider(WorktreeInputs inputs);
  Json create(const std::string& slug);
  bool hasChanges(const std::string& path,const std::string& head)const;
  void remove(const std::string& path,const Json& branch,const Json& root)const;
  void enqueueMetadata(const std::string& id,const Json& data)const;
private:
  process::ExecResult git(const std::string& cwd,
    const std::vector<std::string>& args,bool noPoetry=false)const;
  void postCreationSetup(const std::string& root,const std::string& path,
    const WorktreeSettings& settings)const;
  void copyWorktreeIncludeFiles(const std::string& root,
    const std::string& path)const;
  WorktreeInputs inputs_;
};
void bindBatWorktree(bats::BatGadgetCallServices& services,
  WorktreeInputs inputs);
} // namespace batbox::application_bats
