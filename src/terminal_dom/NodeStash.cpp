#include "terminal_dom/NodeStash.hpp"
#include <iterator>

namespace batbox::terminal_dom {
namespace {
std::weak_ptr<DOMElement> key(DOMElement& node){
  return std::static_pointer_cast<DOMElement>(node.shared_from_this());
}
}
void NodeStash::pruneExpired()const{
  for(auto it=nodeStash_.begin();it!=nodeStash_.end();)
    it=it->first.expired()?nodeStash_.erase(it):std::next(it);
  for(auto it=pendingClears_.begin();it!=pendingClears_.end();)
    it=it->first.expired()?pendingClears_.erase(it):std::next(it);
}
void NodeStash::set(DOMElement& node,StashedRef layout){
  pruneExpired();
  nodeStash_[key(node)]=std::move(layout);
}
StashedRef NodeStash::get(DOMElement& node)const{
  pruneExpired();
  const auto found=nodeStash_.find(key(node));
  return found==nodeStash_.end()?StashedRef{}:found->second;
}
void NodeStash::erase(DOMElement& node){pruneExpired();nodeStash_.erase(key(node));}
void NodeStash::addPendingClear(DOMElement& parent,StashedRef rect,
    bool isAbsolute){
  pruneExpired();
  pendingClears_[key(parent)].push_back(std::move(rect));
  if(isAbsolute)absoluteNodeRemoved_=true;
}
const std::vector<StashedRef>* NodeStash::pendingClears(DOMElement& parent)const{
  pruneExpired();
  const auto found=pendingClears_.find(key(parent));
  return found==pendingClears_.end()?nullptr:&found->second;
}
void NodeStash::erasePendingClears(DOMElement& parent){
  pruneExpired();
  pendingClears_.erase(key(parent));
}
bool NodeStash::consumeAbsoluteRemovedFlag(){
  pruneExpired();
  const auto had=absoluteNodeRemoved_;
  absoluteNodeRemoved_=false;
  return had;
}
} // namespace batbox::terminal_dom
