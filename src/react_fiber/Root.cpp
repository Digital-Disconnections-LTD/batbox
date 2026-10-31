#include "react_fiber/Root.hpp"
#include <stdexcept>

namespace batbox::react_fiber {
StashRef createStash(){return std::make_shared<Stash>();}
void retainStash(const StashRef& stash){
  if(!stash)throw std::invalid_argument("React cache required");
  ++stash->refCount;
}
FiberRootRef createFiberRoot(RootOptions options){
  auto root=std::make_shared<FiberRoot>();
  root->tag=1;
  root->containerInfo=std::move(options.containerInfo);
  root->pendingChildren={};
  root->timeoutHandle=options.noTimeout;
  root->expirationTimes.fill(NoTimestamp);
  root->identifierPrefix=std::move(options.identifierPrefix);
  root->onUncaughtError=std::move(options.onUncaughtError);
  root->onCaughtError=std::move(options.onCaughtError);
  root->onRecoverableError=std::move(options.onRecoverableError);
  root->formState=std::move(options.formState);
  root->current=createHostRootFiber(root->arena,options.tag,
    options.strictMode);
  root->current->rootStateNode=root.get();
  const auto stash=createStash();
  retainStash(stash);
  root->pooledStash=stash;
  retainStash(stash);
  auto state=std::make_shared<RootState>(RootState{
    options.initialChildren?options.initialChildren:react_sdk::nullValue(),
    options.hydrate,stash});
  root->current->memoizedState=state;
  auto queue=std::make_shared<RootUpdateQueue>();
  queue->baseState=state;
  root->current->updateQueue=queue;
  return root;
}
RootStateRef rootState(const FiberRoot& root){
  return root.current?
    std::static_pointer_cast<RootState>(root.current->memoizedState):nullptr;
}
std::shared_ptr<RootUpdateQueue> rootUpdateQueue(const FiberRoot& root){
  return root.current?
    std::static_pointer_cast<RootUpdateQueue>(root.current->updateQueue):nullptr;
}
} // namespace batbox::react_fiber
