#include "terminal_runtime/TerminalControls.hpp"
#include <array>
#include <stdexcept>

namespace batbox::terminal_runtime {
std::function<void()> TerminalControls::patchConsole(
    std::shared_ptr<ConsolePort> console,ConsoleServices services){
  if(!console||!services.format||!services.truthy||!services.logError)
    throw std::invalid_argument("Console imports required");
  static const std::array<const char*,14> stdoutMethods={
    "log","info","debug","dir","dirxml","count","countReset",
    "group","groupCollapsed","groupEnd","table","time","timeEnd",
    "timeLog"};
  static const std::array<const char*,3> stderrMethods={
    "warn","error","trace"};
  auto self=shared_from_this();
  auto formatter=services.format;
  auto logger=services.logError;
  const auto toDebug=std::make_shared<ConsoleMethod>(ConsoleMethod{
    [self,formatter](const ConsoleArgs& args){
      self->services_.logForDebugging("console.log: "+formatter(args),"");
    }});
  const auto toError=std::make_shared<ConsoleMethod>(ConsoleMethod{
    [formatter,logger](const ConsoleArgs& args){
      logger(std::make_exception_ptr(std::runtime_error(
        "console.error: "+formatter(args))));
    }});
  std::vector<std::pair<std::string,ConsoleMethodRef>> originals;
  originals.reserve(stdoutMethods.size()+stderrMethods.size()+1);
  for(const auto* name:stdoutMethods){
    originals.emplace_back(name,console->method(name));
    console->setMethod(name,toDebug);
  }
  for(const auto* name:stderrMethods){
    originals.emplace_back(name,console->method(name));
    console->setMethod(name,toError);
  }
  originals.emplace_back("assert",console->method("assert"));
  console->setMethod("assert",std::make_shared<ConsoleMethod>(ConsoleMethod{
    [truthy=std::move(services.truthy),toError](const ConsoleArgs& args){
      const auto condition=args.empty()?ConsoleArgRef{}:args.front();
      if(!truthy(condition)){
        const auto begin=args.empty()?args.end():args.begin()+1;
        toError->invoke(ConsoleArgs(begin,args.end()));
      }
    }}));
  return [console,originals=std::move(originals)]{
    for(const auto& [name,method]:originals)
      console->setMethod(name,method);
  };
}
} // namespace batbox::terminal_runtime
