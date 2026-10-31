#include "instructions/hooks/runtime/ElicitationHooks.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
hook::HookResult hookResult(const Json& row){
  return hook::HookResult(row.at("command").get<std::string>(),
    row.at("output").get<std::string>(),row.at("succeeded").get<bool>(),
    row.at("blocked").get<bool>());
}
Json responseJson(const hook::ElicitationResponse& response){
  Json value={{"action",response.action}};
  if(response.content)value["content"]=*response.content;
  return value;
}
Json blockingJson(const hook::HookBlockingError& error){
  return {{"blockingError",error.blockingError},{"command",error.command}};
}
Json normalized(const hook::ElicitationParsed& result){
  Json data=Json::object();
  if(result.response)data["response"]=responseJson(*result.response);
  if(result.blockingError)data["blockingError"]=blockingJson(*result.blockingError);
  return {{"data",data},{"ownsResponseContent",
    result.response&&result.response->contentOwnProperty},
    {"ownsBlocking",result.blockingError.has_value()}};
}
Json normalized(const hook::ElicitationHookOutput& result,
  const std::string& kind){
  Json data=Json::object();
  if(result.response)data[kind=="request"?"elicitationResponse":
    "elicitationResultResponse"]=responseJson(*result.response);
  if(result.blockingError)data["blockingError"]=blockingJson(*result.blockingError);
  return {{"data",data},{"ownsResponseContent",false},
    {"ownsBlocking",result.blockingError.has_value()}};
}
Json run(const Json& item){
  event_loop::Loop loop;Json trace=Json::array(),dispatch=nullptr,value=nullptr,error=nullptr;
  const auto kind=item.at("kind").get<std::string>();
  auto parse=[](const Json& input){return hook::hookOutputSchemaSafeParse(input,{});};
  if(kind=="parse"){
    try{value=normalized(hook::parseElicitationHookOutput(
      hookResult(item.at("result")),item.at("event").get<std::string>(),parse));}
    catch(const std::exception& failure){error=failure.what();}
  }else{
    hook::ElicitationHookServices services;services.defaultTimeoutMs=600000;
    services.schemaSafeParse=parse;
    services.createBaseHookInput=[&](std::optional<std::string> permission){
      trace.push_back("base:"+(permission?*permission:"undefined"));
      return hook::BaseHookInput{"s1","/tmp/t","/work",permission};};
    services.executeHooksOutsideRepl=[&](instructions::hooks::HookDispatch call,
      std::optional<std::stop_token>){
      trace.push_back("execute");dispatch={{"hookInput",call.hookInput},
        {"matchQuery",call.matchQuery},{"timeoutMs",call.timeoutMs}};
      event_loop::Deferred<std::vector<hook::HookResult>> output(loop);
      if(item.contains("executionError"))output.reject(std::make_exception_ptr(
        std::runtime_error(item.at("executionError").get<std::string>())));
      else{
        std::vector<hook::HookResult> results;
        for(const auto& row:item.at("results"))results.push_back(hookResult(row));
        output.resolve(std::move(results));
      }
      return output;};
    auto finish=[&](const hook::ElicitationHookOutput* result,
      const std::exception_ptr& failure){
      if(failure)try{std::rethrow_exception(failure);}
        catch(const std::exception& exception){error=exception.what();}
      else value=normalized(*result,kind);};
    if(kind=="request"){
      hook::ElicitationRequest request;
      request.serverName=item.at("serverName").get<std::string>();
      request.message=item.at("message").get<std::string>();
      if(item.contains("requestedSchema"))request.requestedSchema=item.at("requestedSchema");
      if(item.contains("permissionMode"))request.permissionMode=item.at("permissionMode").get<std::string>();
      if(item.contains("mode"))request.mode=item.at("mode").get<std::string>();
      if(item.contains("url"))request.url=item.at("url").get<std::string>();
      if(item.contains("elicitationId"))request.elicitationId=item.at("elicitationId").get<std::string>();
      if(item.contains("timeout"))request.timeoutMs=item.at("timeout").get<double>();
      hook::executeElicitationHooks(loop,services,request).then(finish);
    }else{
      hook::ElicitationResultRequest request;
      request.serverName=item.at("serverName").get<std::string>();
      request.action=item.at("action").get<std::string>();
      if(item.contains("content"))request.content=item.at("content");
      if(item.contains("permissionMode"))request.permissionMode=item.at("permissionMode").get<std::string>();
      if(item.contains("mode"))request.mode=item.at("mode").get<std::string>();
      if(item.contains("elicitationId"))request.elicitationId=item.at("elicitationId").get<std::string>();
      if(item.contains("timeout"))request.timeoutMs=item.at("timeout").get<double>();
      hook::executeElicitationResultHooks(loop,services,request).then(finish);
    }
    loop.run();
  }
  return {{"name",item.at("name")},{"trace",trace},{"dispatch",dispatch},
    {"value",value},{"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream input(argv[1]);Json cases;input>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<rows.dump()<<'\n';
}
