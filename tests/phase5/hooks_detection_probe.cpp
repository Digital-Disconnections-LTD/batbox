#include "instructions/hooks/runtime/AsyncHookDetection.hpp"
#include "runtime/JsonValue.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
Json run(const Json& item){
  event_loop::Loop loop;Json logs=Json::array();
  hook::BackgroundHookRequest request;
  request.processId="async_hook_7";request.hookId="hook-7";
  request.hookEvent="InstructionsLoaded";request.hookName="my-hook";
  request.command="run hook";request.pluginId="plugin-x";
  hook::AsyncDetectionServices services;
  services.logDebug=[&](const std::string& value){logs.push_back(value);};
  services.parseJson=[](const std::string& text){return value::parse(text);};
  services.stringifyJson=[](const Json& value){return value::stringify(value);};
  services.stringifyThrown=[](std::exception_ptr error){
    try{std::rethrow_exception(error);}catch(const std::exception& value){
      return std::string("SyntaxError: ")+value.what();}
  };
  services.executeBackground=[&](hook::BackgroundHookRequest value){
    logs.push_back("background:"+value::stringify(Json{{"processId",value.processId},
      {"asyncResponse",value.asyncResponse}}));
    return item.at("accepted").get<bool>();};
  hook::AsyncHookDetection detection(loop,std::move(request),std::move(services),
    item.at("forceSync").get<bool>());
  Json asyncResult=nullptr;
  detection.asyncResult().then([&](const hook::CommandOutcome* value,
    const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    asyncResult={{"stdout",value->stdoutText},{"stderr",value->stderrText},
      {"output",value->output},{"status",value->status}};});
  for(const auto& chunk:item.at("chunks"))
    detection.onStdout(chunk.get<std::string>());
  loop.run();
  return {{"name",item.at("name")},{"logs",logs},
    {"stdout",detection.stdoutText()},{"stderr",detection.stderrText()},
    {"output",detection.outputText()},
    {"checked",detection.initialResponseChecked()},
    {"transferred",detection.transferred()},{"asyncResult",asyncResult}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<value::stringify(rows)<<'\n';
}
