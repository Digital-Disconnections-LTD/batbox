#include "react_fiber/Root.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
int main(){
  Json rows=Json::array();
  for(int tag:{0,1})for(bool strict:{false,true}){
    const auto container=std::make_shared<std::string>("container");
    react_fiber::RootOptions options;
    options.tag=tag;options.strictMode=strict;
    options.containerInfo=container;
    options.identifierPrefix="id"+std::to_string(tag)+
      (strict?"true":"false");
    auto root=react_fiber::createFiberRoot(options);
    const auto* fiber=root->current;
    const auto state=react_fiber::rootState(*root);
    const auto queue=react_fiber::rootUpdateQueue(*root);
    rows.push_back({{"tag",tag},{"strict",strict},
      {"rootTag",root->tag},{"containerSame",root->containerInfo==container},
      {"currentTag",static_cast<int>(fiber->tag)},
      {"mode",fiber->mode},{"flags",fiber->flags},
      {"lanes",fiber->lanes},{"childLanes",fiber->childLanes},
      {"pendingProps",nullptr},{"stateNodeSame",fiber->rootStateNode==root.get()},
      {"stateElement",nullptr},{"hydrated",state->isDehydrated},
      {"cacheSame",root->pooledStash==state->stash},
      {"cacheRefs",state->stash->refCount},
      {"cacheSize",state->stash->data.size()},
      {"queueStateSame",queue->baseState==state},
      {"queueLanes",queue->shared.lanes},
      {"queuePending",nullptr},{"timeout",root->timeoutHandle},
      {"expirationLength",root->expirationTimes.size()},
      {"firstExpiration",root->expirationTimes.front()},
      {"lastExpiration",root->expirationTimes.back()},
      {"prefix",root->identifierPrefix}});
  }
  std::cout<<rows.dump()<<'\n';
}
