#pragma once
#include "event_loop/Loop.hpp"
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace batbox::application_roost {

// The active working directory and the project that owns it are distinct for
// a resumed worktree. The snapshot is owned by the application event loop.
struct Snapshot {
  std::filesystem::path cwd;
  std::filesystem::path projectRoot;
  Json worktreeFlight;
};
struct Plan {
  Snapshot target;
  bool missingWorktree=false;
};

class Roost final {
public:
  using Rebind=std::function<void(const Snapshot& previous,
    const Snapshot& next)>;
  Roost(event_loop::Loop& owner,std::filesystem::path initialCwd);
  const Snapshot& snapshot()const;
  std::filesystem::path cwd()const;
  std::filesystem::path projectRoot()const;
  Plan planForResume(const Json& log,const std::string& entrypoint)const;
  bool needsRebind(const Plan& plan)const;
  // Changes the process cwd only after a valid target is known. A failed
  // application rebind restores the previous cwd and asks the same callback
  // to restore the previous provider graph.
  void activate(const Plan& plan,const Rebind& rebind);
  void restore(const Snapshot& previous,const Rebind& rebind);
private:
  void requireOwner()const;
  event_loop::Loop& owner_;
  Snapshot current_;
};

} // namespace batbox::application_roost
