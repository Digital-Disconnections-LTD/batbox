#include "terminal_runtime/TerminalControls.hpp"
#include "terminal_runtime/ReactTree.hpp"
#include "terminal_runtime/TerminalStartup.hpp"
#include "react_sdk/Element.hpp"
#include "indigenous_yoga/Node.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>

using namespace batbox;
namespace {
Json* trace=nullptr;
const Json* currentSpec=nullptr;
int nextScreen=0,nextPool=0;
std::map<const terminal_screen::Screen*,int> screenIds;
std::map<const void*,int> poolIds;
}
namespace batbox::terminal_screen {
StylePool::StylePool(){}
std::int32_t StylePool::withInverse(std::int32_t base){return base+1;}
Cell cellAtIndex(const Screen&,double){return {};}
bool isEmptyCellAt(const Screen&,double,double){return true;}
void setCellStyleId(Screen&,double,double,std::int32_t){}
std::optional<Cell> cellAt(const Screen&,double,double){return {};}
double clamp(double value,std::optional<double> low,
    std::optional<double> high){
  if(low&&value<*low)value=*low;
  if(high&&value>*high)value=*high;
  return value;
}
CharPool::CharPool(){poolIds[this]=++nextPool;}
HyperlinkPool::HyperlinkPool(){poolIds[this]=++nextPool;}
ScreenRef createScreen(double width,double height,
    const std::shared_ptr<StylePool>&,
    const std::shared_ptr<CharPool>& chars,
    const std::shared_ptr<HyperlinkPool>& links,
    const DebugWarning&){
  trace->push_back(Json::array({"screen",width,height}));
  auto result=std::make_shared<Screen>();
  result->width=width;result->height=height;
  result->charPool=chars;result->hyperlinkPool=links;
  screenIds[result.get()]=++nextScreen;
  return result;
}
void migrateScreenPools(Screen& screen,
    const std::shared_ptr<CharPool>& chars,
    const std::shared_ptr<HyperlinkPool>& links){
  trace->push_back(Json::array({"migrate",screenIds[&screen]}));
  screen.charPool=chars;screen.hyperlinkPool=links;
}
}
namespace batbox::terminal_log_update {
LogUpdate::LogUpdate(Options options,Services services):
  options_(std::move(options)),services_(std::move(services)){}
void LogUpdate::reset(){trace->push_back(Json::array({"logReset"}));}
terminal_renderer::Diff LogUpdate::renderPreviousOutput_DEPRECATED(
    const terminal_renderer::Frame&){
  trace->push_back(Json::array({"renderPrevious"}));
  if(options_.isTTY)return {};
  return {terminal_renderer::StdoutPatch{"\n"}};
}
terminal_renderer::Diff LogUpdate::render(
    const terminal_renderer::Frame&,const terminal_renderer::Frame&,
    bool altScreen,bool safe){
  trace->push_back(Json::array({"logRender",altScreen,safe}));
  terminal_renderer::Diff diff;
  if(currentSpec->contains("diff"))
    for(const auto& item:currentSpec->at("diff"))
      diff.push_back(terminal_renderer::StdoutPatch{
        item.get<std::string>()});
  return diff;
}
}
namespace batbox::terminal_log_update {
void writeDiffToTerminal(const std::function<void(const std::string&)>& write,
    const terminal_renderer::Diff& diff,bool,
    const termio::OutputServices&){
  trace->push_back(Json::array({"writeDiff",diff.size()}));
  for(const auto& patch:diff)
    if(const auto* text=std::get_if<terminal_renderer::StdoutPatch>(&patch))
      write(text->content);
}
}
namespace batbox::terminal_renderer {
Renderer::Renderer(terminal_dom::DOMElement& node,
    std::shared_ptr<terminal_screen::StylePool> stylePool,
    terminal_dom::NodeStash& stash,RendererServices services):
    node_(node),stylePool_(std::move(stylePool)),stash_(stash),
    services_(std::move(services)){}
Frame Renderer::render(const RenderOptions& options){
  trace->push_back(Json::array({"renderer",options.terminalWidth,
    options.terminalRows,options.altScreen,
    options.prevFrameContaminated}));
  return emptyFrame(options.terminalRows,options.terminalWidth,
    stylePool_,options.backFrame.screen->charPool,
    options.backFrame.screen->hyperlinkPool,{});
}
RendererRef createRenderer(terminal_dom::DOMElement& node,
    std::shared_ptr<terminal_screen::StylePool> pool,
    terminal_dom::NodeStash& stash,RendererServices services){
  return std::make_shared<Renderer>(node,std::move(pool),stash,
    std::move(services));
}
}
namespace batbox::terminal_render {
RenderNodeRenderer::RenderNodeRenderer(RenderState& state,
    RenderNodeImports imports):state_(state),imports_(std::move(imports)){}
void RenderNodeRenderer::render(terminal_dom::DOMElement&,
    terminal_output::Output&,RenderNodeOptions){}
}
namespace batbox::terminal_reconciler {
bool DebugRepaints::enabled(){return false;}
void ProfileCounters::recordYogaMs(double){}
double ProfileCounters::getLastYogaMs()const{return 0;}
double ProfileCounters::getLastCommitMs()const{return 0;}
void ProfileCounters::resetProfileCounters(){
  trace->push_back(Json::array({"resetProfile"}));
}
}
namespace batbox::indigenous_yoga {
Counters getYogaCounters(){return {0,0,0,0};}
}
namespace batbox::terminal_dom {
StashedRef NodeStash::get(DOMElement&)const{return {};}
std::vector<std::string> findOwnerChainAtRow(DOMElement&,double){return {};}
ElementRef createNode(const std::string& name,const DomServicesRef&){
  return std::make_shared<DOMElement>(name);}
}
namespace batbox::terminal_dom {
FocusManager::FocusManager(FocusDispatch dispatchFocusEvent,
    terminal_events::MonotonicNow now):
    dispatchFocusEvent_(std::move(dispatchFocusEvent)),now_(std::move(now)){}
void FocusManager::focusNext(DOMElement&){}
void FocusManager::focusPrevious(DOMElement&){}
}
namespace batbox::terminal_hit_test {
bool dispatchClick(terminal_dom::DOMElement&,double,double,bool,
    const terminal_dom::NodeStash&,const PointerHandlerStore&){return false;}
void dispatchHover(terminal_dom::DOMElement&,double,double,Hovered&,
    const terminal_dom::NodeStash&,const PointerHandlerStore&){}
}
namespace batbox::terminal_selection {
void applySelectionOverlay(terminal_screen::Screen&,
    const SelectionState&,terminal_screen::StylePool&){}
}
namespace batbox::terminal_render {
bool applyPositionedHighlight(terminal_screen::Screen&,
    terminal_screen::StylePool&,const std::vector<MatchPosition>&,
    double,double){return false;}
}
namespace {
struct Output:terminal_runtime::OutputPort {
  bool tty=true;
  double cols=20,rowsValue=6;
  bool isTTY()const override{return tty;}
  std::optional<double> columns()const override{return cols;}
  std::optional<double> rows()const override{return rowsValue;}
  void write(const std::string& data)override{
    trace->push_back(Json::array({"write",data}));}
};
struct Input:terminal_runtime::InputPort {
  bool tty=true,raw=false;
  std::vector<terminal_runtime::ListenerRef> readable;
  std::map<const terminal_runtime::Listener*,int> ids;
  std::vector<std::string> streamReads;
  std::size_t streamIndex=0;
  bool isTTY()const override{return tty;}
  bool isRaw()const override{return raw;}
  bool hasSetRawMode()const override{return true;}
  void setRawMode(bool value)override{
    trace->push_back(Json::array({"raw",value}));raw=value;}
  std::vector<terminal_runtime::ListenerRef> listeners(
      const std::string& event)const override{
    return event=="readable"?readable:
      std::vector<terminal_runtime::ListenerRef>{};
  }
  void removeListener(const std::string& event,
      terminal_runtime::ListenerRef listener)override{
    trace->push_back(Json::array({"remove",event,ids[listener.get()]}));
    const auto it=std::find(readable.begin(),readable.end(),listener);
    if(it!=readable.end())readable.erase(it);
  }
  void addListener(const std::string& event,
      terminal_runtime::ListenerRef listener)override{
    trace->push_back(Json::array({"add",event,ids[listener.get()]}));
    readable.push_back(listener);
  }
  std::optional<std::string> read()override{
    if(streamIndex<streamReads.size()){
      const auto value=streamReads[streamIndex++];
      trace->push_back(Json::array({"streamRead",value}));
      return value;
    }
    trace->push_back(Json::array({"streamRead",nullptr}));
    return {};
  }
};
struct Stderr:terminal_runtime::StderrPort {
  terminal_runtime::StderrWriteRef current;
  Stderr(){
    current=std::make_shared<terminal_runtime::StderrWrite>(
      [](const terminal_runtime::StderrWriteCall& call){
        std::string value;
        if(const auto* string=std::get_if<std::string>(&call.chunk))
          value=*string;
        else{
          const auto& bytes=std::get<std::vector<std::uint8_t>>(call.chunk);
          value=value::decodeUtf8(std::string_view(
            reinterpret_cast<const char*>(bytes.data()),bytes.size()));
        }
        trace->push_back(Json::array({"originalStderr",value}));
        if(call.callback)call.callback();
        return true;
      });
  }
  terminal_runtime::StderrWriteRef writeMethod()const override{return current;}
  void setWriteMethod(terminal_runtime::StderrWriteRef method)override{
    current=std::move(method);}
  bool write(terminal_runtime::StderrWriteCall call){return (*current)(call);}
};
struct StderrOutput:Output,Stderr {
  void write(const std::string& text)override{
    Stderr::write({text,{},{}});
  }
};
struct Console:terminal_runtime::ConsolePort {
  std::map<std::string,terminal_runtime::ConsoleMethodRef> methods;
  Console(){
    for(const auto* name:{"log","info","debug","dir","dirxml",
        "count","countReset","group","groupCollapsed","groupEnd",
        "table","time","timeEnd","timeLog","warn","error",
        "trace","assert"}){
      methods[name]=std::make_shared<terminal_runtime::ConsoleMethod>(
        terminal_runtime::ConsoleMethod{[name](
            const terminal_runtime::ConsoleArgs&){
          trace->push_back(Json::array({"originalConsole",name}));}});
    }
  }
  terminal_runtime::ConsoleMethodRef method(
      const std::string& name)const override{
    const auto it=methods.find(name);
    return it==methods.end()?terminal_runtime::ConsoleMethodRef{}:
      it->second;
  }
  void setMethod(const std::string& name,
      terminal_runtime::ConsoleMethodRef method)override{
    methods[name]=std::move(method);
  }
  void call(const std::string& name,
      const terminal_runtime::ConsoleArgs& args){
    methods.at(name)->invoke(args);
  }
};
struct ReactToken {int id=0;};
Json frame(const terminal_renderer::Frame& value){
  return {{"screen",Json::array({screenIds[value.screen.get()],
      value.screen->width,value.screen->height})},
    {"viewport",{{"width",value.viewport.width},
      {"height",value.viewport.height}}},
    {"cursor",{{"x",value.cursor.x},{"y",value.cursor.y},
      {"visible",value.cursor.visible}}},
    {"charPool",poolIds[value.screen->charPool.get()]},
    {"hyperlinkPool",poolIds[value.screen->hyperlinkPool.get()]},
    {"damage",value.screen->damage?
      Json{{"x",value.screen->damage->x},
        {"y",value.screen->damage->y},
        {"width",value.screen->damage->width},
        {"height",value.screen->damage->height}}:Json(nullptr)}};
}
Json run(const Json& spec){
  Json events=Json::array(),lifecycle=Json::array(),react=Json::array();
  trace=&events;
  currentSpec=&spec;
  event_loop::Loop loop;
  nextScreen=0;nextPool=0;screenIds.clear();poolIds.clear();
  Output stdout;
  stdout.tty=spec.value("tty",true);
  stdout.cols=spec.value("columns",20.0);
  stdout.rowsValue=spec.value("rows",6.0);
  Input stdin;
  auto stderr=std::make_shared<Stderr>();
  auto console=std::make_shared<Console>();
  stdin.tty=spec.value("stdinTTY",true);
  stdin.raw=spec.value("raw",false);
  for(int i=1;i<=spec.value("listeners",0);++i){
    auto listener=std::make_shared<terminal_runtime::Listener>();
    stdin.ids[listener.get()]=i;stdin.readable.push_back(listener);
  }
  if(spec.contains("streamReads"))
    for(const auto& item:spec.at("streamReads"))
      stdin.streamReads.push_back(item.get<std::string>());
  int kernelRead=0;
  terminal_runtime::ControlServices services;
  services.flushSyncFromReconciler=[&]{events.push_back(Json::array({"flush"}));};
  services.onRender=[&]{events.push_back(Json::array({"onRender"}));};
  services.resetLog=[&]{events.push_back(Json::array({"logReset"}));};
  services.renderCurrentNode=[&](terminal_runtime::ReactNodeRef node){
    events.push_back(Json::array({"renderCurrent",
      std::static_pointer_cast<ReactToken>(node)->id}));};
  services.supportsExtendedKeys=[&]{return spec.value("extended",false);};
  services.logForDebugging=[&](const std::string& text,
      const std::string& level){
    events.push_back(Json::array({"debug",text,level}));
    if(spec.value("reenter",false)&&text.rfind("[stderr]",0)==0)
      stderr->write({std::string("inner"),{}, {}});
  };
  services.screenDebug=[](const std::string&,const std::string&){};
  services.cancelScheduledRender=[&]{events.push_back(Json::array({"cancel"}));};
  services.ownerLoop=&loop;
  services.drain.windows=spec.value("windows",false);
  services.drain.openNonblockingTty=[&](){
    events.push_back(Json::array({"ttyOpen"}));
    if(spec.value("openThrows",false))throw std::runtime_error("no tty");
    return 9;
  };
  services.drain.readTty=[&](int,char*,std::size_t)->std::ptrdiff_t{
    int count=0;
    if(spec.contains("kernelReads")&&
        kernelRead<static_cast<int>(spec.at("kernelReads").size()))
      count=spec.at("kernelReads")[kernelRead].get<int>();
    ++kernelRead;
    events.push_back(Json::array({"ttyRead",count}));
    if(count<0)throw std::runtime_error("empty");
    return count;
  };
  services.drain.closeTty=[&](int fd){
    events.push_back(Json::array({"ttyClose",fd}));};
  auto stylePool=std::make_shared<terminal_screen::StylePool>();
  auto charPool=std::make_shared<terminal_screen::CharPool>();
  auto hyperlinkPool=std::make_shared<terminal_screen::HyperlinkPool>();
  auto inkOwner=std::make_shared<terminal_runtime::TerminalControls>(
    stdout,stdin,stylePool,charPool,hyperlinkPool,services);
  auto& ink=*inkOwner;
  terminal_runtime::InteractionServices interaction;
  interaction.scheduleRender=[&]{events.push_back(Json::array({"onRender"}));};
  ink.bindInteraction(std::move(interaction));
  std::function<void()> restoreStderr,restoreConsole;
  terminal_runtime::ConsoleServices consoleServices;
  consoleServices.format=[](const terminal_runtime::ConsoleArgs& args){
    std::string result;
    for(const auto& arg:args){
      if(!result.empty())result+=' ';
      const auto value=std::static_pointer_cast<Json>(arg);
      if(!value)result+="undefined";
      else if(value->is_string())result+=value->get<std::string>();
      else result+=value->dump();
    }
    return result;
  };
  consoleServices.truthy=[](const terminal_runtime::ConsoleArgRef& arg){
    const auto value=std::static_pointer_cast<Json>(arg);
    if(!value||value->is_null())return false;
    if(value->is_boolean())return value->get<bool>();
    if(value->is_number())return value->get<double>()!=0;
    if(value->is_string())return !value->get<std::string>().empty();
    return true;
  };
  consoleServices.logError=[&](std::exception_ptr error){
    try{std::rethrow_exception(error);}
    catch(const std::exception& ex){
      events.push_back(Json::array({"logError",ex.what()}));}
  };
  terminal_runtime::ShutdownServices shutdown;
  auto shutdownLog=std::make_unique<terminal_log_update::LogUpdate>(
    terminal_log_update::Options{stdout.tty,stylePool},
    terminal_log_update::Services{});
  shutdown.log=shutdownLog.get();
  shutdown.sequences={"\x1b[?1006l\x1b[?1003l\x1b[?1002l\x1b[?1000l",
    "\x1b[?1049l","\x1b[>4m","\x1b[<u","\x1b[?1004l",
    "\x1b[?2004l","\x1b[?25h","\x1b]9;4;0;\x07",
    "\x1b]21337;indicator=;status=;status-color=\x07",""};
  shutdown.writeSyncStdout=[&](const std::string& data){
    events.push_back(Json::array({"syncWrite",1,data}));};
  shutdown.supportsTabStatus=[&]{return spec.value("tabStatus",false);};
  shutdown.wrapForMultiplexer=[&](const std::string& seq){
    events.push_back(Json::array({"wrapMultiplexer",seq}));
    return "mux"+seq;
  };
  shutdown.unsubscribeExit=[&]{events.push_back(Json::array({"unsubscribeExit"}));};
  shutdown.updateContainerNull=[&]{events.push_back(Json::array({"updateContainerNull"}));};
  shutdown.flushSyncWork=[&]{events.push_back(Json::array({"flushSyncWork"}));};
  shutdown.deleteInstance=[&]{events.push_back(Json::array({"deleteInstance"}));};
  auto ownerRoot=std::make_shared<terminal_dom::DOMElement>("root");
  shutdown.rootNode=ownerRoot;
  ink.bindShutdown(std::move(shutdown));
  terminal_runtime::ListenerRef resize,cont;
  if(spec.value("lifecycle",false)){
    terminal_runtime::ProcessLifecycleServices process;
    process.onExit=[&](terminal_runtime::ListenerRef,bool alwaysLast){
      lifecycle.push_back(Json::array({"onExit",alwaysLast}));
      return [&lifecycle]{lifecycle.push_back(Json::array({"offExit"}));};
    };
    process.onStdoutResize=[&](terminal_runtime::ListenerRef listener){
      resize=listener;
      lifecycle.push_back(Json::array({"onStdout","resize"}));};
    process.offStdoutResize=[&](terminal_runtime::ListenerRef listener){
      lifecycle.push_back(Json::array({"offStdout","resize",
        listener==resize}));};
    process.onSigcont=[&](terminal_runtime::ListenerRef listener){
      cont=listener;
      lifecycle.push_back(Json::array({"onProcess","SIGCONT"}));};
    process.offSigcont=[&](terminal_runtime::ListenerRef listener){
      lifecycle.push_back(Json::array({"offProcess","SIGCONT",
        listener==cont}));};
    ink.bindProcessLifecycle(std::move(process));
  }
  terminal_dom::NodeStash renderStash;
  terminal_dom::DOMElement renderRoot("root");
  terminal_renderer::Renderer renderer(renderRoot,stylePool,renderStash,{});
  terminal_render::RenderState renderState(renderStash);
  terminal_reconciler::ProfileCounters profile([]{return 0.0;});
  terminal_reconciler::DebugRepaints debugRepaints([]{
    return std::optional<std::string>{};});
  terminal_dom::FocusManager focusManager(
    [](terminal_dom::DOMElement&,terminal_events::FocusEvent&){return true;},
    []{return 0.0;});
  std::vector<react_sdk::ValueRef> firstCallbacks;
  react_sdk::ValueRef lastReactTree;
  if(spec.value("reactTree",false)){
    terminal_runtime::ReactTreeServices tree;
    tree.appComponent=react_sdk::string("App");
    tree.terminalWriteProvider=react_sdk::string("TerminalWriteProvider");
    tree.stdin=std::shared_ptr<terminal_runtime::InputPort>(&stdin,
      [](terminal_runtime::InputPort*){});
    tree.stdout=std::shared_ptr<terminal_runtime::OutputPort>(&stdout,
      [](terminal_runtime::OutputPort*){});
    tree.stderr=std::shared_ptr<terminal_runtime::OutputPort>(&stdout,
      [](terminal_runtime::OutputPort*){});
    tree.exitOnCtrlC=false;
    tree.createContainer=[&](const terminal_runtime::FiberCreateArgs& args){
      react.push_back(Json::array({"createContainer",args.rootTag,
        args.strictMode,args.identifierPrefix,
        args.onUncaughtError&&args.onCaughtError&&
        args.onRecoverableError&&args.onDefaultTransitionIndicator}));
      return std::static_pointer_cast<void>(std::make_shared<int>(1));
    };
    tree.updateContainerSync=[&](const terminal_runtime::FiberUpdateArgs& update){
      const auto& node=update.node;
      if(!node||node->kind==react_sdk::Kind::Null){
        react.push_back(Json::array({"update",nullptr}));return;
      }
      lastReactTree=node;
      const auto& element=*node->element;
      const auto& props=*element.props;
      const auto providerValue=props.get("children");
      const auto& provider=*providerValue->element;
      const auto& providerProps=*provider.props;
      const auto child=providerProps.get("children");
      const auto token=std::static_pointer_cast<ReactToken>(child->hostObject);
      const std::vector<std::string> names={"onExit",
        "onSelectionChange","onClickAt","onHoverAt","getHyperlinkAt",
        "onOpenHyperlink","onMultiClick","onSelectionDrag",
        "onStdinResume","onCursorDeclaration","dispatchKeyboardEvent"};
      std::vector<react_sdk::ValueRef> callbacks;
      Json callbackTypes=Json::array(),same=Json::array();
      for(const auto& name:names)callbacks.push_back(props.get(name));
      callbacks.push_back(providerProps.get("value"));
      for(std::size_t i=0;i<callbacks.size();++i){
        callbackTypes.push_back("function");
        same.push_back(firstCallbacks.empty()||
          callbacks[i]==firstCallbacks[i]);
      }
      if(firstCallbacks.empty())firstCallbacks=callbacks;
      Json keys=Json::array(),providerKeys=Json::array();
      for(const auto& field:props.fields)keys.push_back(field.name);
      for(const auto& field:providerProps.fields)
        providerKeys.push_back(field.name);
      const auto streams=Json::array({
        props.get("stdin")->hostObject.get()==&stdin,
        props.get("stdout")->hostObject.get()==&stdout,
        props.get("stderr")->hostObject.get()==&stdout});
      Json summary={{"type",element.type->text},{"keys",keys},
        {"provider",provider.type->text},{"providerKeys",providerKeys},
        {"node",token->id},{"streams",streams},
        {"selection",props.get("selection")->hostObject.get()==
          &ink.selection()},
        {"exitOnCtrlC",props.get("exitOnCtrlC")->boolean},
        {"columns",props.get("terminalColumns")->number},
        {"rows",props.get("terminalRows")->number},
        {"callbackTypes",callbackTypes},{"same",same}};
      react.push_back(Json::array({"update",summary}));
    };
    tree.flushSyncWork=[&]{react.push_back(Json::array({"flush"}));};
    tree.flushSyncFromReconciler=[&]{
      events.push_back(Json::array({"flush"}));};
    ink.bindOwnerRoot(terminal_runtime::OwnerRootServices{
      ownerRoot,&focusManager,&profile,[]{return 0.0;},
      [&]{events.push_back(Json::array({"onRender"}));}});
    ink.bindReactTree(std::move(tree));
  }
  terminal_runtime::RenderCycleServices renderCycle;
  renderCycle.flushInteractionTime=[&]{
    events.push_back(Json::array({"flushInteractionTime"}));};
  renderCycle.performanceNow=[]{return 0.0;};
  renderCycle.renderer=&renderer;
  renderCycle.renderState=&renderState;
  renderCycle.log=shutdownLog.get();
  renderCycle.profile=&profile;
  renderCycle.debugRepaints=&debugRepaints;
  renderCycle.syncOutputSupported=spec.value("safe",false);
  if(spec.value("onFrame",false))
    renderCycle.onFrame=[&](const terminal_renderer::FrameEvent& event){
      events.push_back(Json::array({"onFrame",
        event.phases?event.phases->patches:0,
        event.flickers.size()}));};
  std::map<int,std::function<void()>> selectionUnsub;
  std::map<int,terminal_dom::ElementRef> cursorNodes;
  const auto cursorNode=[&](int id)->terminal_dom::ElementRef{
    auto& node=cursorNodes[id];
    if(!node)node=std::make_shared<terminal_dom::DOMElement>("cursor");
    return node;
  };
  for(const auto& op:spec.at("ops")){
    const auto name=op.at(0).get<std::string>();
    if(name=="setDimensions"){
      stdout.cols=op.at(1).get<double>();
      stdout.rowsValue=op.at(2).get<double>();
    }else if(name=="setCurrentNode")
      ink.setCurrentNode(std::make_shared<ReactToken>(ReactToken{
        op.at(1).get<int>()}));
    else if(name=="setDisplayCursor")
      ink.setDisplayCursor(terminal_runtime::CursorPoint{
        op.at(1).get<double>(),op.at(2).get<double>()});
    else if(name=="pause")ink.pause();
    else if(name=="resume")ink.resume();
    else if(name=="repaint")ink.repaint();
    else if(name=="forceRedraw")ink.forceRedraw();
    else if(name=="invalidatePrevFrame")ink.invalidatePrevFrame();
    else if(name=="setAltScreenActive")
      ink.setAltScreenActive(op.at(1).get<bool>(),
        op.size()>2?op.at(2).get<bool>():false);
    else if(name=="reassertTerminalModes")
      ink.reassertTerminalModes(op.size()>1?op.at(1).get<bool>():false);
    else if(name=="enterAlternateScreen")ink.enterAlternateScreen();
    else if(name=="exitAlternateScreen")ink.exitAlternateScreen();
    else if(name=="handleResume")ink.handleResume();
    else if(name=="handleResize")ink.handleResize();
    else if(name=="suspendStdin")ink.suspendStdin();
    else if(name=="resumeStdin")ink.resumeStdin();
    else if(name=="drainStdin")ink.drainStdin();
    else if(name=="detachForShutdown")ink.detachForShutdown();
    else if(name=="writeRaw")ink.writeRaw(op.at(1).get<std::string>());
    else if(name=="renderTree")ink.render(react_sdk::toOpaqueNode(
      react_sdk::hostObject(std::make_shared<ReactToken>(ReactToken{
        op.at(1).get<int>()}))));
    else if(name=="callReact"){
      const auto callbackName=op.at(1).get<std::string>();
      const auto& props=*lastReactTree->element->props;
      auto callback=props.get(callbackName);
      if(callbackName=="writeRaw")
        callback=props.get("children")->element->props->get("value");
      std::vector<react_sdk::ValueRef> args;
      for(std::size_t i=2;i<op.size();++i){
        const auto& value=op.at(i);
        if(value.is_string())args.push_back(
          react_sdk::string(value.get<std::string>()));
        else if(value.is_boolean())args.push_back(
          react_sdk::boolean(value.get<bool>()));
        else if(value.is_number())args.push_back(
          react_sdk::number(value.get<double>()));
        else args.push_back(react_sdk::nullValue());
      }
      const auto result=callback->function->invoke(args);
      Json value=nullptr;
      if(result&&result->kind==react_sdk::Kind::Boolean)
        value=result->boolean;
      else if(result&&result->kind==react_sdk::Kind::String)
        value=result->text;
      else if(result&&result->kind==react_sdk::Kind::Number)
        value=result->number;
      react.push_back(Json::array({"call",callbackName,value}));
    }
    else if(name=="resetLineCount")ink.resetLineCount();
    else if(name=="resetPools")ink.resetPools();
    else if(name=="subscribeSelection"){
      const int id=op.at(1).get<int>();
      selectionUnsub[id]=ink.subscribeToSelectionChange(
        std::make_shared<terminal_runtime::SelectionListener>(
          terminal_runtime::SelectionListener{[&,id]{
            events.push_back(Json::array({"selectionListener",id}));}}));
    }else if(name=="unsubscribeSelection")
      selectionUnsub.at(op.at(1).get<int>())();
    else if(name=="declareCursor"){
      ink.setCursorDeclaration(
        std::make_shared<terminal_runtime::CursorDeclaration>(
          terminal_runtime::CursorDeclaration{
            op.at(2).get<double>(),op.at(3).get<double>(),
            cursorNode(op.at(1).get<int>())}));
    }else if(name=="clearCursor")ink.setCursorDeclaration({});
    else if(name=="clearCursorWithNode")ink.setCursorDeclaration({},
      op.at(1).is_null()?terminal_dom::ElementRef{}:
        cursorNode(op.at(1).get<int>()));
    else if(name=="setHyperlinkCallback")
      ink.setHyperlinkClick([&](const std::string& url){
        events.push_back(Json::array({"hyperlinkClick",url}));});
    else if(name=="openHyperlink")
      ink.openHyperlink(op.at(1).get<std::string>());
    else if(name=="setSearchHighlight")
      ink.setSearchHighlight(op.at(1).get<std::string>());
    else if(name=="setSearchPositions"){
      if(op.at(1).is_null())ink.setSearchPositions({});
      else{
        auto state=std::make_shared<terminal_runtime::SearchPositions>();
        state->rowOffset=op.at(1).at("rowOffset").get<double>();
        state->currentIdx=op.at(1).at("currentIdx").get<double>();
        for(const auto& item:op.at(1).at("positions"))
          state->positions.push_back(terminal_render::MatchPosition{
            item.at("row").get<double>(),item.at("col").get<double>(),
            item.at("len").get<double>()});
        ink.setSearchPositions(state);
      }
    }
    else if(name=="waitUntilExit"){
      const int id=op.at(1).get<int>();
      ink.waitUntilExit().then([&events,id](const event_loop::Unit*,
          const std::exception_ptr& error){
        if(error){
          try{std::rethrow_exception(error);}
          catch(const std::exception& ex){
            events.push_back(Json::array({"exitRejected",id,ex.what()}));}
        }else events.push_back(Json::array({"exitResolved",id}));
      });
    }else if(name=="resolveExitPromise")ink.resolveExitPromise();
    else if(name=="rejectExitPromise")
      ink.rejectExitPromise(std::make_exception_ptr(
        std::runtime_error(op.at(1).get<std::string>())));
    else if(name=="unmount")ink.unmount();
    else if(name=="unmountError")ink.unmount(std::make_exception_ptr(
      std::runtime_error(op.at(1).get<std::string>())));
    else if(name=="enableRenderCycle")ink.bindRenderCycle(renderCycle);
    else if(name=="onRender")ink.onRender();
    else if(name=="bindOwnerRoot")ink.bindOwnerRoot(
      terminal_runtime::OwnerRootServices{ownerRoot,&focusManager,&profile,
        []{return 0.0;},[&]{events.push_back(Json::array({"onRender"}));}});
    else if(name=="bindNativeScheduler")ink.bindIndigenousRenderScheduler();
    else if(name=="scheduleRender")ink.scheduleRender();
    else if(name=="rootCompute")ownerRoot->onComputeLayout();
    else if(name=="rootSchedule")ownerRoot->onRender();
    else if(name=="patchStderr")restoreStderr=ink.patchStderr(stderr);
    else if(name=="restoreStderr")restoreStderr();
    else if(name=="writeStderr")stderr->write({
      op.at(1).get<std::string>(),{},[&]{
        events.push_back(Json::array({"stderrCallback"}));}});
    else if(name=="writeStderrBytes"){
      std::vector<std::uint8_t> bytes;
      for(const auto& item:op.at(1))bytes.push_back(item.get<std::uint8_t>());
      stderr->write({std::move(bytes),{},[&]{
        events.push_back(Json::array({"stderrCallback"}));}});
    }
    else if(name=="replaceStderr")stderr->setWriteMethod(
      std::make_shared<terminal_runtime::StderrWrite>(
        [](const terminal_runtime::StderrWriteCall& call){
          trace->push_back(Json::array({"thirdpartyStderr",
            std::get<std::string>(call.chunk)}));
          if(call.callback)call.callback();
          return true;
        }));
    else if(name=="patchConsole")
      restoreConsole=ink.patchConsole(console,consoleServices);
    else if(name=="restoreConsole")restoreConsole();
    else if(name=="callConsole"){
      terminal_runtime::ConsoleArgs args;
      for(std::size_t i=2;i<op.size();i++)
        args.push_back(std::make_shared<Json>(op.at(i)));
      console->call(op.at(1).get<std::string>(),args);
    }
    else if(name=="handleSelectionDrag")
      ink.handleSelectionDrag(op.at(1).get<double>(),op.at(2).get<double>());
    else if(name=="moveSelectionFocus"){
      static const std::map<std::string,terminal_selection::FocusMove> moves={
        {"left",terminal_selection::FocusMove::Left},
        {"right",terminal_selection::FocusMove::Right},
        {"up",terminal_selection::FocusMove::Up},
        {"down",terminal_selection::FocusMove::Down},
        {"lineStart",terminal_selection::FocusMove::LineStart},
        {"lineEnd",terminal_selection::FocusMove::LineEnd}};
      ink.moveSelectionFocus(moves.at(op.at(1).get<std::string>()));
    }else if(name=="clearTextSelection")ink.clearTextSelection();
    else if(name=="shiftSelectionForScroll")
      ink.shiftSelectionForScroll(op.at(1).get<double>(),
        op.at(2).get<double>(),op.at(3).get<double>());
    else if(name=="setSelectionPoints"){
      auto& state=ink.selection();
      terminal_selection::startSelection(state,op.at(1).get<double>(),
        op.at(2).get<double>());
      terminal_selection::updateSelection(state,op.at(3).get<double>(),
        op.at(4).get<double>());
    }
    else throw std::logic_error("unknown ink control operation");
  }
  loop.runReady();
  Json listenerIds=Json::array();
  for(const auto& value:stdin.readable)
    listenerIds.push_back(stdin.ids[value.get()]);
  Json cursor=nullptr;
  if(const auto value=ink.displayCursor())
    cursor={{"x",value->x},{"y",value->y}};
  Json declaredCursor=nullptr;
  if(const auto& declaration=ink.cursorDeclaration()){
    int id=0;
    for(const auto& [candidate,node]:cursorNodes)
      if(node==declaration->node){id=candidate;break;}
    declaredCursor={{"relativeX",declaration->relativeX},
      {"relativeY",declaration->relativeY},{"node",id}};
  }
  Json searchPositions=nullptr;
  if(const auto& state=ink.searchPositions()){
    Json positions=Json::array();
    for(const auto& item:state->positions)
      positions.push_back({{"row",item.row},{"col",item.col},
        {"len",item.len}});
    searchPositions={{"positions",positions},
      {"rowOffset",state->rowOffset},{"currentIdx",state->currentIdx}};
  }
  return {{"trace",events},{"lifecycle",lifecycle},{"react",react},
    {"front",frame(ink.frontFrame())},
    {"back",frame(ink.backFrame())},
    {"columns",ink.terminalColumns()},{"rows",ink.terminalRows()},
    {"park",ink.altScreenParkPatch()},
    {"paused",ink.isPaused()},{"unmounted",ink.isUnmounted()},
    {"alt",ink.isAltScreenActive()},
    {"contaminated",ink.prevFrameContaminated()},
    {"erase",ink.needsEraseBeforePaint()},
    {"cursor",cursor},{"raw",stdin.raw},
    {"listeners",listenerIds},
    {"stored",ink.storedStdinListeners()},
    {"wasRaw",ink.wasRawMode()},
    {"selection",{{"anchor",ink.selection().anchor?
        Json{{"col",ink.selection().anchor->col},
             {"row",ink.selection().anchor->row}}:Json(nullptr)},
      {"focus",ink.selection().focus?
        Json{{"col",ink.selection().focus->col},
             {"row",ink.selection().focus->row}}:Json(nullptr)},
      {"dragging",ink.selection().isDragging},
      {"span",nullptr},{"has",ink.hasTextSelection()}}},
    {"declaredCursor",declaredCursor},
    {"searchHighlightQuery",ink.searchHighlightQuery()},
    {"searchPositions",searchPositions}};
}
Json runStartup(const Json& spec){
  Json events=Json::array(),lifecycle=Json::array(),react=Json::array();
  trace=&events;
  nextScreen=0;nextPool=0;screenIds.clear();poolIds.clear();
  event_loop::Loop loop;
  auto stdout=std::make_shared<Output>();
  auto stdin=std::make_shared<Input>();
  auto stderr=std::make_shared<StderrOutput>();
  auto console=std::make_shared<Console>();
  stdout->tty=spec.value("tty",true);
  stdin->tty=spec.value("stdinTTY",true);
  terminal_runtime::StartupOptions options;
  options.stdout=stdout;
  options.stdin=stdin;
  options.stderr=stderr;
  options.exitOnCtrlC=false;
  options.patchConsole=spec.value("startupPatch",false);
  terminal_runtime::StartupServices services{loop};
  services.performanceNow=[]{return 0.0;};
  services.logForDebugging=[&](const std::string& message,
      const std::string& level){
    events.push_back(Json::array({"debug",message,level}));};
  services.screenDebug=[](const std::string&,const std::string&){};
  services.supportsExtendedKeys=[](){return false;};
  services.flushInteractionTime=[]{};
  services.processLifecycle.onExit=[&](terminal_runtime::ListenerRef,bool last){
    lifecycle.push_back(Json::array({"onExit",last}));
    return [&lifecycle]{lifecycle.push_back(Json::array({"offExit"}));};
  };
  services.processLifecycle.onStdoutResize=[&](terminal_runtime::ListenerRef){
    lifecycle.push_back(Json::array({"onStdout","resize"}));};
  services.processLifecycle.offStdoutResize=[](terminal_runtime::ListenerRef){};
  services.processLifecycle.onSigcont=[&](terminal_runtime::ListenerRef){
    lifecycle.push_back(Json::array({"onProcess","SIGCONT"}));};
  services.processLifecycle.offSigcont=[](terminal_runtime::ListenerRef){};
  services.console=console;
  services.consoleServices.format=[](const terminal_runtime::ConsoleArgs& args){
    if(args.empty())return std::string{};
    return std::static_pointer_cast<Json>(args.front())->get<std::string>();};
  services.consoleServices.truthy=[](const terminal_runtime::ConsoleArgRef& arg){
    return arg&&std::static_pointer_cast<Json>(arg)->get<bool>();};
  services.consoleServices.logError=[](std::exception_ptr){};
  services.dom=std::make_shared<terminal_dom::DomServices>();
  services.dom->createLayoutNode=[](){return terminal_dom::LayoutRef{};};
  services.dispatcher=std::make_shared<terminal_events::Dispatcher>(
    terminal_events::Priorities{1,2,3,4},[](std::exception_ptr){});
  services.monotonicNow=[]{return 0.0;};
  services.renderNodeImports.widestLine=[](const std::string&){return 0.0;};
  services.outputRender.widestLine=[](const std::string&){return 0.0;};
  services.outputRender.sliceAnsi=[](const std::string& text,double,double){
    return text;};
  services.chalk=std::make_shared<terminal_color::ChalkState>();
  services.setClipboard=[&loop](const std::string&){
    return event_loop::Deferred<std::string>(loop);};
  services.unhandledClipboardRejection=[](std::exception_ptr){};
  services.debugRepaintsEnv=[](){return std::optional<std::string>{};};
  services.oscModule=std::make_shared<termio::OscModule>([]{return "xterm";});
  services.output.getClearTerminalSequence=[](){return std::string{};};
  services.output.link=[](const std::string& url){return url;};
  services.writeSyncStdout=[](const std::string&){};
  services.supportsTabStatus=[](){return false;};
  services.wrapForMultiplexer=[](const std::string& text){return text;};
  services.deleteInstance=[]{};
  services.react.appComponent=react_sdk::string("App");
  services.react.terminalWriteProvider=
    react_sdk::string("TerminalWriteProvider");
  services.react.createContainer=[&](const terminal_runtime::FiberCreateArgs& args){
    react.push_back(Json::array({"createContainer",args.rootTag,
      args.strictMode,args.identifierPrefix,
      args.onUncaughtError&&args.onCaughtError&&
      args.onRecoverableError&&args.onDefaultTransitionIndicator}));
    return std::static_pointer_cast<void>(std::make_shared<int>(1));};
  services.react.updateContainerSync=[](const terminal_runtime::FiberUpdateArgs&){};
  services.react.flushSyncWork=[]{};
  services.react.flushSyncFromReconciler=[]{};
  auto owner=terminal_runtime::createTerminalControls(
    std::move(options),std::move(services));
  for(const auto& op:spec.at("ops")){
    const auto name=op.at(0).get<std::string>();
    if(name=="callConsole"){
      terminal_runtime::ConsoleArgs args;
      for(std::size_t i=2;i<op.size();++i)
        args.push_back(std::make_shared<Json>(op.at(i)));
      console->call(op.at(1).get<std::string>(),args);
    }else if(name=="writeStderr")
      stderr->Stderr::write({op.at(1).get<std::string>(),{},[&]{
        events.push_back(Json::array({"stderrCallback"}));}});
    else throw std::logic_error("Unsupported startup operation");
  }
  return {{"trace",events},{"lifecycle",lifecycle},{"react",react}};
}
}
int main(){
  const std::string input(std::istreambuf_iterator<char>(std::cin),{});
  const auto cases=value::parse(input);
  Json output=Json::array();
  for(const auto& spec:cases)
    output.push_back(spec.value("startup",false)?runStartup(spec):run(spec));
  std::cout<<value::stringify(output)<<'\n';
}
