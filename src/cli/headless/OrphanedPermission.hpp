#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>

namespace batbox::cli::headless {
struct OrphanedPermissionState {
  std::unordered_set<std::string> handledGadgetUseIds;
};
struct OrphanedPermissionServices {
  std::function<void(const std::string&)> logForDebugging;
  std::function<event_loop::Deferred<Json>(const std::string&)>
    findUnresolvedGadgetUse;
  std::function<void(const Json&)> enqueue;
  std::function<void()> onEnqueued;
};
event_loop::Deferred<bool> handleOrphanedPermissionResponse(
  event_loop::Loop& loop,const Json& message,
  std::shared_ptr<OrphanedPermissionState> state,
  const OrphanedPermissionServices& services);
}
