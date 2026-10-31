#pragma once
#include "react_fiber/Fiber.hpp"
#include <array>
#include <functional>
#include <map>
#include <unordered_map>

namespace batbox::react_fiber {
constexpr std::size_t TotalLanes=31;
constexpr int NoTimestamp=-1;
struct Stash {
  bool aborted=false;
  std::unordered_map<const void*,std::shared_ptr<void>> data;
  int refCount=0;
};
using StashRef=std::shared_ptr<Stash>;
StashRef createStash();
void retainStash(const StashRef& stash);
struct RootState {
  ValueRef element;
  bool isDehydrated=false;
  StashRef stash;
};
using RootStateRef=std::shared_ptr<RootState>;
struct RootUpdateQueue {
  RootStateRef baseState;
  std::shared_ptr<void> firstBaseUpdate,lastBaseUpdate;
  struct Shared {
    std::shared_ptr<void> pending;
    Lanes lanes=0;
    std::shared_ptr<void> hiddenCallbacks;
  } shared;
  std::shared_ptr<void> callbacks;
};
struct RootOptions {
  std::shared_ptr<void> containerInfo;
  int tag=1;
  bool hydrate=false,strictMode=false;
  ValueRef initialChildren;
  std::string identifierPrefix;
  ValueRef formState;
  ValueRef onUncaughtError,onCaughtError,onRecoverableError;
  int noTimeout=-1;
};
struct FiberRoot {
  FiberArena arena;
  int tag=1;
  std::shared_ptr<void> containerInfo;
  ValueRef pendingChildren;
  Fiber* current=nullptr;
  std::shared_ptr<void> pingStash;
  int timeoutHandle=-1;
  std::shared_ptr<void> cancelPendingCommit,context,pendingContext,next;
  std::shared_ptr<void> callbackNode;
  Lanes callbackPriority=0;
  std::array<double,TotalLanes> expirationTimes{};
  Lanes pendingLanes=0,suspendedLanes=0,pingedLanes=0,warmLanes=0;
  Lanes expiredLanes=0,errorRecoveryDisabledLanes=0;
  int shellSuspendCounter=0;
  Lanes entangledLanes=0;
  std::array<Lanes,TotalLanes> entanglements{};
  std::array<std::shared_ptr<void>,TotalLanes> hiddenUpdates{};
  std::string identifierPrefix;
  ValueRef onUncaughtError,onCaughtError,onRecoverableError;
  StashRef pooledStash;
  Lanes pooledStashLanes=0;
  ValueRef formState;
  std::map<const void*,std::shared_ptr<void>> incompleteTransitions;
};
using FiberRootRef=std::shared_ptr<FiberRoot>;
FiberRootRef createFiberRoot(RootOptions options);
RootStateRef rootState(const FiberRoot& root);
std::shared_ptr<RootUpdateQueue> rootUpdateQueue(const FiberRoot& root);
} // namespace batbox::react_fiber
