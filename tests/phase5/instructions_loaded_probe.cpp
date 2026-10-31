#include "instructions/hooks/InstructionsLoaded.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <iterator>
#include <optional>
#include <stdexcept>

using namespace batbox;
namespace hooks=batbox::instructions::hooks;

int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream stream(argv[1]);
  const std::string data(std::istreambuf_iterator<char>{stream},{});
  Json rows=Json::array();
  for(const auto& item:value::parse(data)){
    event_loop::Loop loop;
    Json logs=Json::array();
    std::optional<hooks::HookDispatch> dispatch;
    hooks::InstructionsLoadedImports imports;
    imports.defaultTimeoutMs=1200;
    imports.getHooksConfigFromSnapshot=[&]() -> std::optional<Json>{
      logs.push_back("snapshot");
      if(item.at("snapshot").is_null())return std::nullopt;
      return Json{{"InstructionsLoaded",item.at("snapshot")}};};
    imports.getRegisteredHooks=[&]() -> std::optional<Json>{
      logs.push_back("registered");
      if(item.at("registered").is_null())return std::nullopt;
      return Json{{"InstructionsLoaded",item.at("registered")}};};
    imports.createBaseHookInput=[&]{logs.push_back("base:undefined");
      return item.contains("base")?item.at("base"):Json{{"session_id","local"}};};
    imports.executeHooksOutsideRepl=[&](hooks::HookDispatch value){
      logs.push_back("execute");dispatch=std::move(value);
      event_loop::Deferred<Json> result(loop);
      if(item.value("reject",false))result.reject(
        std::make_exception_ptr(std::runtime_error("hook failed")));
      else result.resolve(Json::object());
      return result;};
    const bool has=hooks::hasInstructionsLoadedHook(imports);
    hooks::InstructionsLoadedOptions options;
    const auto& input=item.at("options");
    if(input.contains("globs"))options.globs=input.at("globs").get<std::vector<std::string>>();
    if(input.contains("triggerFilePath"))options.triggerFilePath=input.at("triggerFilePath").get<std::string>();
    if(input.contains("parentFilePath"))options.parentFilePath=input.at("parentFilePath").get<std::string>();
    if(input.contains("timeoutMs"))options.timeoutMs=input.at("timeoutMs").get<double>();
    auto pending=hooks::executeInstructionsLoadedHooks(loop,imports,
      "/work/INSTRUCTIONS.md","Project","session_start",options);
    logs.push_back("after-call");
    loop.queueMicrotask([&]{logs.push_back("sibling-microtask");});
    std::string outcome="pending";
    pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
      if(!error){outcome="resolved";return;}
      try{std::rethrow_exception(error);}catch(const std::exception& failure){
        outcome=failure.what();}});
    loop.runReady();
    if(outcome=="resolved")logs.push_back("completion");
    if(!dispatch)throw std::runtime_error("Missing hook dispatch");
    rows.push_back({{"name",item.at("name")},{"has",has},
      {"outcome",outcome},{"dispatch",Json{{"hookInput",dispatch->hookInput},
        {"ownUndefinedKeys",dispatch->ownUndefinedKeys},
        {"timeoutMs",dispatch->timeoutMs},{"matchQuery",dispatch->matchQuery}}},
      {"logs",logs}});
  }
  std::cout<<value::stringify(rows)<<'\n';
}
