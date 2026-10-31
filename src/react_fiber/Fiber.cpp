#include "react_fiber/Fiber.hpp"

namespace batbox::react_fiber {
Fiber* FiberArena::create(WorkTag tag,ValueRef pendingProps,
    std::optional<std::string> key,std::uint32_t mode){
  auto fiber=std::make_unique<Fiber>();
  fiber->tag=tag;fiber->pendingProps=std::move(pendingProps);
  fiber->key=std::move(key);fiber->mode=mode;
  auto* result=fiber.get();fibers_.push_back(std::move(fiber));return result;
}
Fiber* createWorkInProgress(FiberArena& arena,Fiber& current,
    ValueRef pendingProps){
  auto* work=current.alternate;
  if(!work){
    work=arena.create(current.tag,std::move(pendingProps),current.key,
      current.mode);
    work->elementType=current.elementType;
    work->type=current.type;
    work->stateNode=current.stateNode;
    work->rootStateNode=current.rootStateNode;
    work->alternate=&current;
    current.alternate=work;
  }else{
    work->pendingProps=std::move(pendingProps);
    work->type=current.type;
    work->flags=NoFlags;
    work->subtreeFlags=NoFlags;
    work->deletions.clear();
  }
  work->flags=current.flags&StaticMask;
  work->childLanes=current.childLanes;
  work->lanes=current.lanes;
  work->child=current.child;
  work->memoizedProps=current.memoizedProps;
  work->memoizedState=current.memoizedState;
  work->updateQueue=current.updateQueue;
  work->dependencies=current.dependencies?
    std::make_shared<Dependencies>(*current.dependencies):nullptr;
  work->sibling=current.sibling;
  work->index=current.index;
  work->ref=current.ref;
  work->refCleanup=current.refCleanup;
  return work;
}
Fiber& resetWorkInProgress(Fiber& work,Lanes renderLanes){
  work.flags&=StaticMask|Placement;
  const auto* current=work.alternate;
  if(!current){
    work.childLanes=0;
    work.lanes=renderLanes;
    work.child=nullptr;
    work.subtreeFlags=NoFlags;
    work.memoizedProps.reset();
    work.memoizedState.reset();
    work.updateQueue.reset();
    work.dependencies.reset();
    work.stateNode.reset();
    work.rootStateNode=nullptr;
  }else{
    work.childLanes=current->childLanes;
    work.lanes=current->lanes;
    work.child=current->child;
    work.subtreeFlags=NoFlags;
    work.deletions.clear();
    work.memoizedProps=current->memoizedProps;
    work.memoizedState=current->memoizedState;
    work.updateQueue=current->updateQueue;
    work.type=current->type;
    work.dependencies=current->dependencies?
      std::make_shared<Dependencies>(*current->dependencies):nullptr;
  }
  return work;
}
Fiber* createHostRootFiber(FiberArena& arena,int,bool strictMode){
  auto mode=ConcurrentMode;
  if(strictMode)mode|=StrictLegacyMode|StrictEffectsMode;
  return arena.create(WorkTag::HostRoot,{},std::nullopt,mode);
}
bool isFunctionClassComponent(const ValueRef& type){
  return type&&type->kind==react_sdk::Kind::Function&&type->function&&
    type->function->isReactComponent;
}
bool isSimpleFunctionComponent(const ValueRef& type){
  return type&&type->kind==react_sdk::Kind::Function&&type->function&&
    !isFunctionClassComponent(type)&&!type->function->defaultProps;
}
} // namespace batbox::react_fiber
