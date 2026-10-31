#include "terminal_reconciler/HostLeaves.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include "utils/envUtils.hpp"
#include <stdexcept>
#include <unordered_set>

namespace batbox::terminal_reconciler {
namespace {
bool strictEqual(const batbox::value::ValueRef& a,
    const batbox::value::ValueRef& b){
  if(a==b)return true;
  if(!a||!b)return false;
  if(a->is_number()&&b->is_number())
    return a->get<double>()==b->get<double>();
  if(a->type()!=b->type())return false;
  if(a->is_structured())return false;
  return *a==*b;
}
}
PropsRef diff(const PropsRef& before,const PropsRef& after){
  if(before==after)return {};
  if(!before)return after;
  auto changed=std::make_shared<Props>();
  bool isChanged=false;
  for(const auto& key:before->keys()){
    const bool isDeleted=!after||!after->has(key);
    if(isDeleted){changed->setRef(key,{});isChanged=true;}
  }
  if(after){
    for(const auto& key:after->keys()){
      if(!strictEqual(after->get(key),before->get(key))){
        changed->setRef(key,after->get(key));isChanged=true;
      }
    }
  }
  return isChanged?changed:PropsRef{};
}
void cleanupYogaNode(terminal_dom::DOMNode& node){
  const auto yoga=std::dynamic_pointer_cast<terminal_dom::FullLayoutNode>(
    node.yogaNode);
  if(node.yogaNode&&!yoga)
    throw std::invalid_argument("Reconciler full Yoga import required");
  if(yoga){
    yoga->unsetMeasureFunc();
    terminal_dom::clearYogaNodeReferences(node);
    yoga->freeRecursive();
  }
}
std::vector<std::string> getOwnerChain(const OwnerFiberRef& fiber){
  std::vector<std::string> chain;
  std::unordered_set<const OwnerFiber*> seen;
  auto current=fiber;
  for(int i=0;current&&i<50;i++){
    if(!seen.insert(current.get()).second)break;
    std::optional<std::string> name;
    if(current->type!=OwnerFiber::Type::String){
      if(current->displayName&&!current->displayName->empty())
        name=current->displayName;
      else if(current->name)name=current->name;
    }
    if(name&&!name->empty()&&
        (chain.empty()||chain.back()!=*name))chain.push_back(*name);
    current=current->debugOwner?current->debugOwner:current->parent;
  }
  return chain;
}
bool DebugRepaints::enabled(){
  if(!stashed_){
    if(!env_)throw std::invalid_argument(
      "Debug repaint environment import required");
    const auto value=env_();
    stashed_=batbox::isEnvTruthy(value?Json(*value):Json(nullptr));
  }
  return *stashed_;
}
void ProfileCounters::recordYogaMs(double value){lastYogaMs_=value;}
double ProfileCounters::getLastYogaMs()const{return lastYogaMs_;}
void ProfileCounters::markCommitStart(){
  if(!now_)throw std::invalid_argument("Reconciler performance clock required");
  commitStart_=now_();
}
double ProfileCounters::getLastCommitMs()const{return lastCommitMs_;}
void ProfileCounters::resetProfileCounters(){
  lastYogaMs_=0;lastCommitMs_=0;commitStart_=0;
}
void ProfileCounters::finishCommit(){
  if(commitStart_>0){
    if(!now_)throw std::invalid_argument("Reconciler performance clock required");
    lastCommitMs_=now_()-commitStart_;
  }else lastCommitMs_=0;
  commitStart_=0;
}
} // namespace batbox::terminal_reconciler
