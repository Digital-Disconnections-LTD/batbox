#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include "terminal_renderer/Frame.hpp"
#include "terminal_runtime_roster/Instances.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace batbox::terminal_root {
template<class Output,class Input>struct RenderOptions {
  std::shared_ptr<Output> stdout,stderr;
  std::shared_ptr<Input> stdin;
  std::optional<bool> exitOnCtrlC,patchConsole;
  std::function<void(const terminal_renderer::FrameEvent&)> onFrame;
};
template<class Output,class Input>struct TerminalOptions {
  std::shared_ptr<Output> stdout,stderr;
  std::shared_ptr<Input> stdin;
  bool exitOnCtrlC=true,patchConsole=true;
  std::function<void(const terminal_renderer::FrameEvent&)> onFrame;
};
template<class Output,class Input>using RenderInput=std::variant<
  std::monostate,std::shared_ptr<Output>,RenderOptions<Output,Input>>;

template<class Controller>struct Instance {
  std::function<void(terminal_runtime::ReactNodeRef)> rerender;
  std::function<void()> unmount;
  std::function<event_loop::Deferred<event_loop::Unit>()> waitUntilExit;
  std::function<void()> cleanup;
};
template<class Controller>struct Root {
  std::function<void(terminal_runtime::ReactNodeRef)> render;
  std::function<void()> unmount;
  std::function<event_loop::Deferred<event_loop::Unit>()> waitUntilExit;
};
template<class Output,class Input,class Controller>struct Services {
  event_loop::Loop& loop;
  terminal_runtime_roster::Instances<Output,Controller>& instances;
  std::shared_ptr<Output> processStdout,processStderr;
  std::shared_ptr<Input> processStdin;
  std::function<std::shared_ptr<Controller>(const TerminalOptions<Output,Input>&)> createTerminal;
  std::function<void(const std::shared_ptr<Controller>&,
    terminal_runtime::ReactNodeRef)> renderTerminal;
  std::function<void(const std::shared_ptr<Controller>&)> unmountTerminal;
  std::function<event_loop::Deferred<event_loop::Unit>(
    const std::shared_ptr<Controller>&)> waitUntilTerminalExit;
  std::function<void(const std::string&)> logForDebugging;
  std::function<double()> processUptimeSeconds;
};
using TerminalServices=Services<terminal_runtime::OutputPort,
  terminal_runtime::InputPort,terminal_runtime::TerminalControls>;
using TerminalRenderOptions=RenderOptions<terminal_runtime::OutputPort,
  terminal_runtime::InputPort>;
using TerminalInstance=Instance<terminal_runtime::TerminalControls>;
using TerminalRoot=Root<terminal_runtime::TerminalControls>;

template<class Output,class Input>RenderOptions<Output,Input> getOptions(
    const RenderInput<Output,Input>& input,
    const std::shared_ptr<Input>& processStdin){
  if(const auto stream=std::get_if<std::shared_ptr<Output>>(&input)){
    RenderOptions<Output,Input> options;
    options.stdout=*stream;
    options.stdin=processStdin;
    return options;
  }
  if(const auto options=std::get_if<RenderOptions<Output,Input>>(&input))
    return *options;
  return {};
}

template<class Output,class Input,class Controller>std::shared_ptr<Controller> getInstance(
    const std::shared_ptr<Output>& stdout,
    const std::function<std::shared_ptr<Controller>()>& createInstance,
    terminal_runtime_roster::Instances<Output,Controller>& instances){
  auto instance=instances.get(stdout);
  if(!instance){
    instance=createInstance();
    instances.set(stdout,instance);
  }
  return instance;
}

template<class Output,class Input,class Controller>Instance<Controller> renderSync(
    terminal_runtime::ReactNodeRef node,
    const RenderInput<Output,Input>& input,
    std::shared_ptr<Services<Output,Input,Controller>> services){
  if(!services||!services->createTerminal||!services->renderTerminal||
      !services->unmountTerminal||!services->waitUntilTerminalExit)
    throw std::logic_error("Terminal renderer imports required");
  const auto opts=getOptions(input,services->processStdin);
  TerminalOptions<Output,Input> terminalOptions{
    opts.stdout?opts.stdout:services->processStdout,
    opts.stderr?opts.stderr:services->processStderr,
    opts.stdin?opts.stdin:services->processStdin,
    opts.exitOnCtrlC.value_or(true),opts.patchConsole.value_or(true),
    opts.onFrame};
  auto instance=getInstance<Output,Input,Controller>(terminalOptions.stdout,
    [&](){return services->createTerminal(terminalOptions);},services->instances);
  services->renderTerminal(instance,node);
  return {
    [services,instance](terminal_runtime::ReactNodeRef next){
      services->renderTerminal(instance,next);},
    [services,instance](){services->unmountTerminal(instance);},
    [services,instance](){return services->waitUntilTerminalExit(instance);},
    [services,stdout=terminalOptions.stdout](){services->instances.erase(stdout);}
  };
}

template<class Output,class Input,class Controller>
event_loop::Deferred<Instance<Controller>> wrappedRender(
    terminal_runtime::ReactNodeRef node,
    RenderInput<Output,Input> input,
    std::shared_ptr<Services<Output,Input,Controller>> services){
  if(!services||!services->loop.isOwnerThread())
    throw std::logic_error("render requires owner loop");
  event_loop::Deferred<Instance<Controller>> result(services->loop);
  services->loop.queueMicrotask([node,input=std::move(input),services,result]{
    try{
      auto instance=renderSync(node,input,services);
      if(!services->logForDebugging||!services->processUptimeSeconds)
        throw std::logic_error("Render diagnostics imports required");
      services->logForDebugging("[render] first ink render: "+
        value::numberToString(value::round(services->processUptimeSeconds()*1000))+
        "ms since process start");
      result.resolve(std::move(instance));
    }catch(...){result.reject(std::current_exception());}
  });
  return result;
}

template<class Output,class Input,class Controller>
event_loop::Deferred<Root<Controller>> createRoot(
    RenderOptions<Output,Input> options,
    std::shared_ptr<Services<Output,Input,Controller>> services){
  if(!services||!services->loop.isOwnerThread())
    throw std::logic_error("createRoot requires owner loop");
  event_loop::Deferred<Root<Controller>> result(services->loop);
  services->loop.queueMicrotask([options=std::move(options),services,result]{
    try{
      if(!services->createTerminal||!services->renderTerminal||!services->unmountTerminal||
          !services->waitUntilTerminalExit)
        throw std::logic_error("Terminal renderer imports required");
      TerminalOptions<Output,Input> terminalOptions{
        options.stdout?options.stdout:services->processStdout,
        options.stderr?options.stderr:services->processStderr,
        options.stdin?options.stdin:services->processStdin,
        options.exitOnCtrlC.value_or(true),
        options.patchConsole.value_or(true),options.onFrame};
      auto instance=services->createTerminal(terminalOptions);
      services->instances.set(terminalOptions.stdout,instance);
      result.resolve({
        [services,instance](terminal_runtime::ReactNodeRef node){
          services->renderTerminal(instance,node);},
        [services,instance](){services->unmountTerminal(instance);},
        [services,instance](){return services->waitUntilTerminalExit(instance);}
      });
    }catch(...){result.reject(std::current_exception());}
  });
  return result;
}
} // namespace batbox::terminal_root
