#include "terminal_runtime/TerminalControls.hpp"
#include "terminal_runtime/ReactTree.hpp"
#include "react_sdk/Element.hpp"
#include <stdexcept>
#include <unordered_map>

namespace batbox::terminal_runtime {
using react_sdk::ValueRef;
using ReactArgs=std::vector<ValueRef>;
using ReactCall=std::function<ValueRef(const ReactArgs&)>;

struct ReactTreeState {
  ReactTreeServices services;
  FiberContainerRef container;
  ReactNoopRef noop;
  ValueRef stdin,stdout,stderr,selection;
  std::unordered_map<std::string,ValueRef> callbacks;
  ValueRef callback(const std::string& key)const{
    return callbacks.at(key);
  }
};
namespace {
ValueRef at(const ReactArgs& args,std::size_t index){
  if(index>=args.size())throw std::invalid_argument("React callback argument required");
  return args[index];
}
double asNumber(const ReactArgs& args,std::size_t index){
  const auto value=at(args,index);
  if(!value||value->kind!=react_sdk::Kind::Number)
    throw std::invalid_argument("React numeric argument required");
  return value->number;
}
std::string asString(const ReactArgs& args,std::size_t index){
  const auto value=at(args,index);
  if(!value||value->kind!=react_sdk::Kind::String)
    throw std::invalid_argument("React string argument required");
  return value->text;
}
template<class T>std::shared_ptr<T> asHost(const ReactArgs& args,
    std::size_t index){
  const auto value=at(args,index);
  if(!value||value->kind!=react_sdk::Kind::HostObject)
    throw std::invalid_argument("React host object argument required");
  return std::static_pointer_cast<T>(value->hostObject);
}
ValueRef makeCallback(const std::string& name,ReactCall call){
  auto stored=std::make_shared<ReactCall>(std::move(call));
  auto value=react_sdk::hostFunction(stored,name);
  value->function->invoke=*stored;
  return value;
}
}
void TerminalControls::bindReactTree(ReactTreeServices services){
  if(reactTree_)throw std::logic_error("React tree already bound");
  if(!interaction_||!interaction_->rootNode||
     !services.appComponent||!services.terminalWriteProvider||
     !services.stdin||!services.stdout||!services.stderr||
     !services.createContainer||!services.updateContainerSync||
     !services.flushSyncWork||!services.flushSyncFromReconciler)
    throw std::invalid_argument("React tree and Fiber imports required");
  if(services.stdin.get()!=&stdin_||services.stdout.get()!=&stdout_)
    throw std::invalid_argument("React stream identity mismatch");
  const auto self=shared_from_this();
  auto state=std::make_shared<ReactTreeState>();
  state->services=std::move(services);
  state->stdin=react_sdk::hostObject(state->services.stdin);
  state->stdout=react_sdk::hostObject(state->services.stdout);
  state->stderr=react_sdk::hostObject(state->services.stderr);
  state->selection=react_sdk::hostObject(
    std::shared_ptr<void>(shared_from_this(),&selection_));
  const auto add=[&](const std::string& name,ReactCall call){
    state->callbacks.emplace(name,makeCallback(name,std::move(call)));
  };
  add("onExit",[self](const ReactArgs& args){
    auto owner=self;
    std::exception_ptr error;
    if(!args.empty()&&args[0]&&
       args[0]->kind==react_sdk::Kind::HostObject){
      const auto value=std::static_pointer_cast<ReactError>(
        args[0]->hostObject);
      if(value)error=value->error;
    }
    owner->unmount(error);return react_sdk::undefined();
  });
  add("onSelectionChange",[self](const ReactArgs&){
    self->notifySelectionChange();
    return react_sdk::undefined();
  });
  add("onClickAt",[self](const ReactArgs& args){
    return react_sdk::boolean(self->dispatchClick(
      asNumber(args,0),asNumber(args,1)));
  });
  add("onHoverAt",[self](const ReactArgs& args){
    self->dispatchHover(
      asNumber(args,0),asNumber(args,1));
    return react_sdk::undefined();
  });
  add("getHyperlinkAt",[self](const ReactArgs& args){
    const auto url=self->getHyperlinkAt(asNumber(args,0),asNumber(args,1));
    return url?react_sdk::string(*url):react_sdk::undefined();
  });
  add("onOpenHyperlink",[self](const ReactArgs& args){
    self->openHyperlink(asString(args,0));
    return react_sdk::undefined();
  });
  add("onMultiClick",[self](const ReactArgs& args){
    self->handleMultiClick(asNumber(args,0),
      asNumber(args,1),static_cast<int>(asNumber(args,2)));
    return react_sdk::undefined();
  });
  add("onSelectionDrag",[self](const ReactArgs& args){
    self->handleSelectionDrag(
      asNumber(args,0),asNumber(args,1));
    return react_sdk::undefined();
  });
  add("onStdinResume",[self](const ReactArgs& args){
    self->reassertTerminalModes(
      !args.empty()&&react_sdk::truthy(args[0]));
    return react_sdk::undefined();
  });
  add("onCursorDeclaration",[self](const ReactArgs& args){
    {
      CursorDeclarationRef declaration;
      if(!args.empty()&&args[0]&&
         args[0]->kind==react_sdk::Kind::HostObject)
        declaration=asHost<CursorDeclaration>(args,0);
      std::optional<terminal_dom::ElementRef> clearIfNode;
      if(args.size()>1&&args[1]){
        if(args[1]->kind==react_sdk::Kind::HostObject)
          clearIfNode=asHost<terminal_dom::DOMElement>(args,1);
        else if(args[1]->kind==react_sdk::Kind::Null)
          clearIfNode=terminal_dom::ElementRef{};
      }
      self->setCursorDeclaration(std::move(declaration),
        std::move(clearIfNode));
    }
    return react_sdk::undefined();
  });
  add("dispatchKeyboardEvent",[self](const ReactArgs& args){
    self->dispatchKeyboardEvent(*asHost<terminal_events::ParsedKey>(args,0));
    return react_sdk::undefined();
  });
  add("writeRaw",[self](const ReactArgs& args){
    self->writeRaw(asString(args,0));
    return react_sdk::undefined();
  });
  state->noop=std::make_shared<std::function<void()>>([]{});
  FiberCreateArgs create;
  create.rootNode=interaction_->rootNode;
  create.rootTag=state->services.concurrentRootTag;
  create.onUncaughtError=state->noop;
  create.onCaughtError=state->noop;
  create.onRecoverableError=state->noop;
  create.onDefaultTransitionIndicator=state->noop;
  state->container=state->services.createContainer(create);
  if(!state->container)
    throw std::logic_error("React Fiber container required");
  reactTree_=state;
  services_.flushSyncFromReconciler=
    state->services.flushSyncFromReconciler;
  services_.renderCurrentNode=[this](ReactNodeRef node){render(std::move(node));};
  installReactShutdown();
}
void TerminalControls::installReactShutdown(){
  if(!reactTree_||!shutdown_)return;
  auto state=reactTree_;
  shutdown_->updateContainerNull=[state]{
    state->services.updateContainerSync(FiberUpdateArgs{
      react_sdk::nullValue(),state->container,nullptr,state->noop});};
  shutdown_->flushSyncWork=state->services.flushSyncWork;
}
void TerminalControls::render(ReactNodeRef node){
  if(!reactTree_)throw std::logic_error("React tree not bound");
  currentNode_=std::move(node);
  auto& state=*reactTree_;
  auto child=currentNode_?react_sdk::fromOpaqueNode(currentNode_):
    react_sdk::nullValue();
  auto providerProps=react_sdk::props();
  providerProps->set("value",state.callback("writeRaw"));
  auto provider=react_sdk::createElement(
    state.services.terminalWriteProvider,providerProps,{child});
  auto appProps=react_sdk::props();
  appProps->set("stdin",state.stdin);
  appProps->set("stdout",state.stdout);
  appProps->set("stderr",state.stderr);
  appProps->set("exitOnCtrlC",react_sdk::boolean(
    state.services.exitOnCtrlC));
  appProps->set("onExit",state.callback("onExit"));
  appProps->set("terminalColumns",react_sdk::number(terminalColumns_));
  appProps->set("terminalRows",react_sdk::number(terminalRows_));
  appProps->set("selection",state.selection);
  for(const auto* name:{"onSelectionChange","onClickAt","onHoverAt",
      "getHyperlinkAt","onOpenHyperlink","onMultiClick",
      "onSelectionDrag","onStdinResume","onCursorDeclaration",
      "dispatchKeyboardEvent"})
    appProps->set(name,state.callback(name));
  auto tree=react_sdk::createElement(state.services.appComponent,
    appProps,{react_sdk::element(provider)});
  state.services.updateContainerSync(FiberUpdateArgs{
    react_sdk::element(tree),state.container,nullptr,state.noop});
  state.services.flushSyncWork();
}
} // namespace batbox::terminal_runtime
