#pragma once
#include "react_sdk/Value.hpp"
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace batbox::react_fiber {
struct FiberRoot;
using ValueRef=react_sdk::ValueRef;
using Lanes=std::uint32_t;
using Flags=std::uint32_t;
enum class WorkTag:int {
  FunctionComponent=0,ClassComponent=1,HostRoot=3,HostPortal=4,
  HostComponent=5,HostText=6,Fragment=7,Mode=8,ContextConsumer=9,
  ContextProvider=10,ForwardRef=11,Profiler=12,Suspense=13,
  Memo=14,SimpleMemo=15,Lazy=16,IncompleteClass=17,
  DehydratedFragment=18,SuspenseList=19,Scope=21,Offscreen=22,
  LegacyHidden=23,Stash=24,TracingMarker=25,HostHoistable=26,
  HostSingleton=27,IncompleteFunction=28,Throw=29,
  ViewTransition=30,Activity=31
};
constexpr std::uint32_t ConcurrentMode=1,ProfileMode=2,
  StrictLegacyMode=8,StrictEffectsMode=16,SuspenseyImagesMode=32;
constexpr Flags NoFlags=0,Placement=2,
  LayoutStatic=1u<<22,PassiveStatic=1u<<23,
  MaySuspendCommit=1u<<24,ViewTransitionStatic=1u<<25,
  SnapshotStatic=1u<<21;
constexpr Flags StaticMask=LayoutStatic|PassiveStatic|MaySuspendCommit|
  ViewTransitionStatic|SnapshotStatic;
struct Dependencies {
  Lanes lanes=0;
  std::shared_ptr<void> firstContext;
};
struct Fiber {
  WorkTag tag=WorkTag::FunctionComponent;
  std::optional<std::string> key;
  ValueRef elementType,type,pendingProps,memoizedProps,ref;
  std::shared_ptr<void> stateNode,refCleanup,updateQueue,memoizedState;
  FiberRoot* rootStateNode=nullptr;
  std::shared_ptr<Dependencies> dependencies;
  Fiber* parent=nullptr;
  Fiber* child=nullptr;
  Fiber* sibling=nullptr;
  Fiber* alternate=nullptr;
  std::size_t index=0;
  std::uint32_t mode=0;
  Flags flags=NoFlags,subtreeFlags=NoFlags;
  std::vector<Fiber*> deletions;
  Lanes lanes=0,childLanes=0;
};
class FiberArena {
public:
  Fiber* create(WorkTag tag,ValueRef pendingProps,
    std::optional<std::string> key,std::uint32_t mode);
private:
  std::vector<std::unique_ptr<Fiber>> fibers_;
};
Fiber* createWorkInProgress(FiberArena& arena,Fiber& current,
  ValueRef pendingProps);
Fiber& resetWorkInProgress(Fiber& workInProgress,Lanes renderLanes);
Fiber* createHostRootFiber(FiberArena& arena,int rootTag,bool strictMode);
bool isSimpleFunctionComponent(const ValueRef& type);
bool isFunctionClassComponent(const ValueRef& type);
} // namespace batbox::react_fiber
