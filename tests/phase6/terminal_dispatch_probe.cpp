#include "terminal_events/Dispatcher.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace te=batbox::terminal_events;
namespace {
const te::Priorities priorities{10,20,30,0};
const te::MonotonicNow now=[]{return 0.0;};
struct Tree {
  te::EventTarget root,parent,target;
  Tree(){parent.parentNode=&root;target.parentNode=&parent;}
  std::string name(te::EventTarget* node)const{
    if(node==&root)return "root";
    if(node==&parent)return "parent";
    if(node==&target)return "target";
    return "";
  }
};
std::function<void(std::exception_ptr)> logger(Json& trace){
  return [&trace](std::exception_ptr error){
    try{std::rethrow_exception(error);}
    catch(const std::exception& cause){trace.push_back(
      std::string("error:")+cause.what());}
  };
}
void setup(Tree& tree,Json& trace,
    std::function<void(te::TerminalEvent&)> action={}){
  for(auto [node,label]:std::vector<std::pair<te::EventTarget*,std::string>>{
      {&tree.root,"root"},{&tree.parent,"parent"},{&tree.target,"target"}}){
    node->eventHandlers["onKeyDownCapture"]=[&,label,action](auto& event){
      trace.push_back(label+"Cap:"+event.eventPhase()+":"+
        tree.name(event.currentTarget()));
      if(label=="target"&&action)action(event);};
    node->eventHandlers["onKeyDown"]=[&,label](auto& event){
      trace.push_back(label+"Bub:"+event.eventPhase()+":"+
        tree.name(event.currentTarget()));};
  }
}
Json dispatchCase(const std::string& kind,
    std::function<void(te::TerminalEvent&)> action={}){
  Json trace=Json::array();Tree tree;setup(tree,trace,std::move(action));
  te::Dispatcher dispatcher(priorities,logger(trace));
  te::TerminalEvent event("keydown",now);
  const bool result=dispatcher.dispatch(tree.target,event);
  return Json{{"kind",kind},{"trace",trace},{"result",result},
    {"phase",event.eventPhase()},{"current",event.currentTarget()==nullptr},
    {"target",event.target()==&tree.target},
    {"currentEvent",dispatcher.currentEvent==nullptr}};
}
Json handlerThrow(){
  Json trace=Json::array();Tree tree;setup(tree,trace);
  tree.target.eventHandlers["onKeyDownCapture"]=[&](auto& event){
    trace.push_back("targetCap:"+event.eventPhase()+":"+
      tree.name(event.currentTarget()));
    throw std::runtime_error("handler failed");};
  te::Dispatcher dispatcher(priorities,logger(trace));
  te::TerminalEvent event("keydown",now);
  const bool result=dispatcher.dispatch(tree.target,event);
  return Json{{"kind","handlerThrow"},{"trace",trace},
    {"result",result},{"phase",event.eventPhase()},
    {"current",event.currentTarget()==nullptr},
    {"currentEvent",dispatcher.currentEvent==nullptr}};
}
Json invalidHandler(){
  Json trace=Json::array();Tree tree;setup(tree,trace);
  tree.target.eventHandlers["onKeyDown"]=Json("not callable");
  te::Dispatcher dispatcher(priorities,logger(trace));
  te::TerminalEvent event("keydown",now);
  const bool result=dispatcher.dispatch(tree.target,event);
  return Json{{"kind","invalidHandler"},{"trace",trace},
    {"result",result}};
}
class PrepareEvent:public te::TerminalEvent {
public:
  PrepareEvent(Tree& tree,Json& trace):TerminalEvent("keydown",now),
    tree_(tree),trace_(trace){}
  void prepareForTarget(te::EventTarget* node)override{
    const auto name=tree_.name(node);
    trace_.push_back("prepare:"+name);
    if(name=="target")throw std::runtime_error("prepare failed");
  }
private:
  Tree& tree_;Json& trace_;
};
Json prepareThrow(){
  Json trace=Json::array();Tree tree;setup(tree,trace);
  te::Dispatcher dispatcher(priorities,logger(trace));
  PrepareEvent event(tree,trace);
  std::string error;
  try{(void)dispatcher.dispatch(tree.target,event);}
  catch(const std::exception& cause){error=cause.what();}
  const auto current=event.currentTarget();
  return Json{{"kind","prepareThrow"},{"trace",trace},
    {"error",error},{"phase",event.eventPhase()},
    {"current",current?Json(tree.name(current)):Json(nullptr)},
    {"currentEvent",dispatcher.currentEvent==nullptr}};
}
Json nonbubble(){
  Json trace=Json::array();Tree tree;setup(tree,trace);
  te::Dispatcher dispatcher(priorities,logger(trace));
  te::TerminalEvent event("keydown",now,te::TerminalEventInit{false,false});
  event.preventDefault();
  const bool result=dispatcher.dispatch(tree.target,event);
  return Json{{"kind","nonbubble"},{"trace",trace},
    {"result",result},{"prevented",event.defaultPrevented()}};
}
Json priority(){
  Json trace=Json::array();
  te::Dispatcher dispatcher(priorities,logger(trace));
  te::TerminalEvent event("keydown",now);
  dispatcher.currentUpdatePriority=0;
  const int without=dispatcher.resolveEventPriority();
  dispatcher.currentEvent=&event;
  const int withEvent=dispatcher.resolveEventPriority();
  dispatcher.currentUpdatePriority=47;
  const int explicitValue=dispatcher.resolveEventPriority();
  return Json{{"kind","priority"},{"without",without},
    {"withEvent",withEvent},{"explicit",explicitValue},
    {"propNames",Json::array({te::isEventHandlerProp("onKeyDown"),
      te::isEventHandlerProp("onMouseEnter"),
      te::isEventHandlerProp("onKeyUp")})}};
}
Json continuous(){
  Json trace=Json::array();Tree tree;
  te::Dispatcher dispatcher(priorities,logger(trace));
  tree.target.eventHandlers["onKeyDown"]=[&](auto& event){
    trace.push_back("handler:"+std::to_string(
      dispatcher.currentUpdatePriority));
    event.preventDefault();};
  dispatcher.currentUpdatePriority=45;
  te::TerminalEvent event("keydown",now);
  const bool result=dispatcher.dispatchContinuous(tree.target,event);
  return Json{{"kind","continuous"},{"trace",trace},
    {"result",result},{"priority",dispatcher.currentUpdatePriority}};
}
Json discrete(){
  Json trace=Json::array();Tree tree;
  te::Dispatcher dispatcher(priorities,logger(trace));
  tree.target.eventHandlers["onKeyDown"]=[&](auto&){
    trace.push_back("handler");};
  dispatcher.discreteUpdates=[&](const te::DispatchThunk& dispatch,
      te::EventTarget& node,te::TerminalEvent& event,
      te::UndefinedArg,te::UndefinedArg){
    trace.push_back("before");trace.push_back("undefinedArgs:true");
    const bool result=dispatch(node,event);
    trace.push_back("after");return result;};
  te::TerminalEvent event("keydown",now);
  const bool result=dispatcher.dispatchDiscrete(tree.target,event);
  return Json{{"kind","discrete"},{"trace",trace},{"result",result}};
}
}
int main(){
  const Json rows=Json::array({dispatchCase("order"),
    dispatchCase("stop",[](auto& event){event.stopPropagation();}),
    dispatchCase("immediate",[](auto& event){event.stopImmediatePropagation();}),
    dispatchCase("prevent",[](auto& event){event.preventDefault();}),
    handlerThrow(),invalidHandler(),prepareThrow(),nonbubble(),priority(),
    continuous(),discrete()});
  std::cout<<rows.dump()<<'\n';
}
