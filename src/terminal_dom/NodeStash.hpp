#pragma once
#include "terminal_dom/Node.hpp"
#include <map>

namespace batbox::terminal_dom {
struct StashedLayout {
  double x=0,y=0,width=0,height=0;
  std::optional<double> top;
};
using StashedRef=std::shared_ptr<StashedLayout>;
class NodeStash {
public:
  void set(DOMElement& node,StashedRef layout);
  StashedRef get(DOMElement& node)const;
  void erase(DOMElement& node);
  void addPendingClear(DOMElement& parent,StashedRef rect,bool isAbsolute);
  const std::vector<StashedRef>* pendingClears(DOMElement& parent)const;
  void erasePendingClears(DOMElement& parent);
  bool consumeAbsoluteRemovedFlag();
private:
  using Key=std::weak_ptr<DOMElement>;
  using StashMap=std::map<Key,StashedRef,std::owner_less<Key>>;
  using ClearMap=std::map<Key,std::vector<StashedRef>,std::owner_less<Key>>;
  void pruneExpired()const;
  mutable StashMap nodeStash_;
  mutable ClearMap pendingClears_;
  bool absoluteNodeRemoved_=false;
};
} // namespace batbox::terminal_dom
