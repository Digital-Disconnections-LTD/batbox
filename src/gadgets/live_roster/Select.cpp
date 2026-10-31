#include "gadgets/live_roster/LiveRoster.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_set>

namespace batbox::gadgets::live_roster {
namespace {
void require(const std::shared_ptr<const Snapshot>& snapshot,
    const CatalogSettings& settings){
  if(!snapshot)throw std::invalid_argument("Live tool snapshot required");
  if(!settings.getDenyRuleForGadget)
    throw std::invalid_argument("Live permission deny-rule binding required");
  if(!settings.localeCompare)
    throw std::invalid_argument("Live locale comparator required");
}
}
Selection select(const std::shared_ptr<const Snapshot>& snapshot,
    const Json& permissionContext,const CatalogSettings& settings){
  require(snapshot,settings);
  Gadgets base=filterGadgetsByDenyRules(snapshot->base,permissionContext,settings);
  Gadgets allowedBase;
  for(const Gadget* gadget:base){
    if(!gadget->isEnabled)throw std::invalid_argument(
      "Tool missing enabled method: "+gadget->name);
    if(gadget->isEnabled())allowedBase.push_back(gadget);
  }
  Gadgets allowedMcp=filterGadgetsByDenyRules(snapshot->mcp,
    permissionContext,settings);
  auto byName=[&](const Gadget* left,const Gadget* right){
    return settings.localeCompare(left->name,right->name)<0;
  };
  std::stable_sort(allowedBase.begin(),allowedBase.end(),byName);
  std::stable_sort(allowedMcp.begin(),allowedMcp.end(),byName);
  Selection result{snapshot,{}};
  std::unordered_set<std::string> seen;
  for(const auto* gadget:allowedBase)
    if(seen.insert(gadget->name).second)result.gadgets.push_back(gadget);
  for(const auto* gadget:allowedMcp)
    if(seen.insert(gadget->name).second)result.gadgets.push_back(gadget);
  return result;
}
Selection selectFull(const std::shared_ptr<const Snapshot>& snapshot,
    const Json& permissionContext,const CatalogSettings& settings,
    const std::function<CatalogImports(const Snapshot&)>& imports){
  require(snapshot,settings);
  if(!imports)throw std::invalid_argument("Full tool-catalog binding required");
  return Selection{snapshot,assembleGadgetPool(permissionContext,snapshot->mcp,
    imports(*snapshot),settings)};
}
std::shared_ptr<const Snapshot> Roster::publish(std::shared_ptr<Snapshot> next){
  if(!next)throw std::invalid_argument("Tool snapshot required");
  std::lock_guard lock(mutex_);
  auto previous=std::move(current_);
  if(previous)retired_.push_back(previous);
  current_=std::move(next);
  return previous;
}
std::shared_ptr<const Snapshot> Roster::current()const{
  std::lock_guard lock(mutex_);
  return current_;
}
std::shared_ptr<const Snapshot> Roster::replaceMcp(std::vector<Gadget> gadgets,
    std::vector<std::shared_ptr<mcp::Connection>> connections){
  std::lock_guard lock(mutex_);
  if(!current_)throw std::logic_error("Base tools must be published before MCP connections");
  auto next=std::make_shared<Snapshot>();
  next->searxng=current_->searxng;next->scrapling=current_->scrapling;
  next->mcpConnections=std::move(connections);
  for(const auto* gadget:current_->base){next->owned.push_back(*gadget);next->base.push_back(&next->owned.back());}
  for(auto& gadget:gadgets){next->owned.push_back(std::move(gadget));next->mcp.push_back(&next->owned.back());}
  retired_.push_back(std::move(current_));current_=next;
  return current_;
}
void Roster::releaseRetiredAtQuiescence(){
  std::lock_guard lock(mutex_);
  retired_.clear();
}
std::size_t Roster::retiredCount()const{
  std::lock_guard lock(mutex_);
  return retired_.size();
}
Selection Roster::selectCurrent(const Json& permissionContext,
    const CatalogSettings& settings)const{
  return select(current(),permissionContext,settings);
}
} // namespace batbox::gadgets::live_roster
