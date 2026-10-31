#include "terminal_root/Root.hpp"
#include <iostream>
#include <string>
#include <vector>

using namespace batbox;
struct Output {std::string name;};
struct Input {std::string name;};
struct Controller {int id=0;};
int main(){
  event_loop::Loop loop;
  terminal_runtime_roster::Instances<Output,Controller> roster;
  std::vector<std::string> events;
  const auto stdout=std::make_shared<Output>(Output{"default"}),
    custom=std::make_shared<Output>(Output{"custom"}),
    stderr=std::make_shared<Output>(Output{"stderr"});
  const auto stdin=std::make_shared<Input>(Input{"stdin"});
  int nextId=0;
  auto services=std::make_shared<terminal_root::Services<Output,Input,Controller>>(
    terminal_root::Services<Output,Input,Controller>{
    .loop=loop,.instances=roster,.processStdout=stdout,
    .processStderr=stderr,.processStdin=stdin,
    .createTerminal=[&](const terminal_root::TerminalOptions<Output,Input>& options){
      const auto controller=std::make_shared<Controller>(Controller{++nextId});
      events.push_back("create"+std::to_string(controller->id)+":"+
        options.stdout->name+":"+options.stdin->name+":"+
        (options.exitOnCtrlC?"true":"false")+":"+
        (options.patchConsole?"true":"false"));
      return controller;},
    .renderTerminal=[&](const std::shared_ptr<Controller>& controller,
        terminal_runtime::ReactNodeRef node){
      events.push_back("render"+std::to_string(controller->id)+":"+
        *std::static_pointer_cast<std::string>(node));},
    .unmountTerminal=[&](const std::shared_ptr<Controller>& controller){
      events.push_back("unmount"+std::to_string(controller->id));},
    .waitUntilTerminalExit=[&](const std::shared_ptr<Controller>& controller){
      events.push_back("wait"+std::to_string(controller->id));
      event_loop::Deferred<event_loop::Unit> completed(loop);
      completed.resolve({});return completed;},
    .logForDebugging=[&](const std::string& line){
      events.push_back("debug:"+line);},
    .processUptimeSeconds=[](){return 1.234;}
  });
  const auto node=[](const char* text)->terminal_runtime::ReactNodeRef{
    return std::make_shared<std::string>(text);};
  using InputValue=terminal_root::RenderInput<Output,Input>;
  auto a=terminal_root::renderSync(node("A"),InputValue{},services);
  a.rerender(node("B"));
  auto b=terminal_root::renderSync(node("C"),InputValue{custom},services);
  terminal_root::renderSync(node("D"),InputValue{},services);
  a.cleanup();
  terminal_root::renderSync(node("E"),InputValue{},services);
  terminal_root::RenderOptions<Output,Input> wrappedOpts;
  wrappedOpts.stdout=custom;wrappedOpts.patchConsole=false;
  auto pending=terminal_root::wrappedRender(node("F"),
    InputValue{wrappedOpts},services);
  events.push_back("afterWrappedCall");
  loop.queueMicrotask([&](){events.push_back("wrappedSibling");});
  std::optional<terminal_root::Instance<Controller>> wrapped;
  pending.then([&](const auto* value,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    wrapped=*value;events.push_back("wrappedSettled");});
  loop.runReady();
  terminal_root::RenderOptions<Output,Input> rootOpts;
  rootOpts.stdout=custom;rootOpts.exitOnCtrlC=false;
  auto rootPending=terminal_root::createRoot(rootOpts,services);
  events.push_back("afterCreateRootCall");
  loop.queueMicrotask([&](){events.push_back("rootSibling");});
  std::optional<terminal_root::Root<Controller>> managed;
  rootPending.then([&](const auto* value,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    managed=*value;events.push_back("rootSettled");});
  loop.runReady();
  managed->render(node("G"));managed->unmount();managed->waitUntilExit();
  b.unmount();b.cleanup();
  events.push_back("mapSize:"+std::to_string(roster.size()));
  wrapped->waitUntilExit();
  Json out=events;
  std::cout<<out.dump()<<'\n';
}
