#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::bats {
struct BatGadgetWorktreeState {
  std::optional<Json> worktreeInfo;
  std::string earlyBatId;
  Json selectedBat;
  std::string description;
};
struct BatGadgetWorktreeServices {
  std::function<bool(const std::string& path,const std::string& headCommit)> hasWorktreeChanges;
  std::function<void(const std::string& path,const Json& branch,const Json& gitRoot)> removeBatWorktree;
  // Schedule the metadata operation without waiting; the bound service logs
  // failures independently of this control flow.
  std::function<void(const std::string& batId,const Json& metadata)> enqueueBatMetadata;
};
Json cleanupBatWorktreeIfNeeded(BatGadgetWorktreeState& state,
  BatGadgetWorktreeServices& services);
} // namespace batbox::bats
