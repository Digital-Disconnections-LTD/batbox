#include "diagnostics/ErrorLogQueue.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <map>

int main(){
  using namespace batbox;
  using namespace batbox::diagnostics;
  const auto input=value::parse(std::string(std::istreambuf_iterator<char>(std::cin),{}));
  std::map<std::string,std::string> env;
  if(input.contains("env"))for(auto it=input["env"].begin();it!=input["env"].end();++it)
    env[it.key()]=it.value().get<std::string>();
  std::vector<std::string> args;
  for(const auto& value:input.value("argv",Json::array()))args.push_back(value.get<std::string>());
  bool hardFail=input.value("hardFail",false),essential=input.value("essential",false);
  const int throwOnSink=input.value("throwOnSink",0);
  Json sinkCalls=Json::array(),output=Json::array();
  std::map<std::string,std::shared_ptr<ErrorValue>> errors;
  ErrorQueueServices services;
  services.hardFailFeature=[&]{return hardFail;};
  services.argv=[&]{return args;};
  services.env=[&](const std::string& name)->std::optional<std::string>{
    const auto it=env.find(name);
    return it==env.end()?std::nullopt:std::optional(it->second);
  };
  services.isEssentialTrafficOnly=[&]{return essential;};
  services.timestampISO=[]{return "2026-03-31T12:34:56.000Z";};
  services.hardFailExit=[](const std::string&){throw std::runtime_error("exit 1");};
  ErrorLogQueue queue(std::move(services));
  auto makeError=[](const Json& spec){
    auto error=std::make_shared<ErrorValue>();
    error->name=spec.value("name",std::string("Error"));
    error->message=spec.value("message",std::string{});
    if(spec.contains("stack"))error->stack=spec["stack"].get<std::string>();
    return error;
  };
  auto call=[&](Json entry){
    sinkCalls.push_back(std::move(entry));
    if(throwOnSink&&sinkCalls.size()==static_cast<std::size_t>(throwOnSink))
      throw std::runtime_error("sink failed");
  };
  for(const auto& action:input["actions"]){
    Json error=nullptr;
    if(action.contains("env"))for(auto it=action["env"].begin();it!=action["env"].end();++it){
      if(it.value().is_null())env.erase(it.key());
      else env[it.key()]=it.value().get<std::string>();
    }
    if(action.contains("argv")){
      args.clear();for(const auto& value:action["argv"])args.push_back(value.get<std::string>());
    }
    if(action.contains("hardFail"))hardFail=action["hardFail"].get<bool>();
    if(action.contains("essential"))essential=action["essential"].get<bool>();
    try{
      const auto kind=action["kind"].get<std::string>();
      if(kind=="logError"||kind=="mcpError"){
        auto value=makeError(action["error"]);
        if(action.contains("id"))errors[action["id"].get<std::string>()]=value;
        if(kind=="logError")queue.logError(std::move(value));
        else queue.logMCPError(action["server"].get<std::string>(),std::move(value));
      }
      if(kind=="mcpUnknown")queue.logMCPUnknownError(
        action["server"].get<std::string>(),action["value"]);
      if(kind=="mcpDebug")queue.logMCPDebug(action["server"].get<std::string>(),
        action["message"].get<std::string>());
      if(kind=="mutate"){
        const auto it=errors.find(action["id"].get<std::string>());
        if(it!=errors.end()){
          it->second->message=action["message"].get<std::string>();
          it->second->stack=action["stack"].get<std::string>();
        }
      }
      if(kind=="attach"){
        ErrorLogCallbacks sink;
        sink.logError=[&](const ErrorValue& value){
          call(Json::array({"error",value.name,value.message,
            value.stack&& !value.stack->empty()?Json(*value.stack):Json(nullptr)}));};
        sink.logMCPError=[&](const std::string& server,const ErrorValue& value){
          call(Json::array({"mcpError",server,Json::array({value.name,value.message,
            value.stack&& !value.stack->empty()?Json(*value.stack):Json(nullptr)})}));};
        sink.logMCPUnknownError=[&](const std::string& server,const Json& value){
          call(Json::array({"mcpError",server,value}));};
        sink.logMCPDebug=[&](const std::string& server,const std::string& message){
          call(Json::array({"mcpDebug",server,message}));};
        sink.getErrorsPath=[]{return "/errors";};
        sink.getMCPLogsPath=[](const std::string& server){return "/mcp/"+server;};
        queue.attachErrorLogSink(std::move(sink));
      }
      if(kind=="reset")queue.resetForTesting();
    }catch(const std::exception& caught){error=caught.what();}
    output.push_back({{"error",error},{"inMemory",queue.getInMemoryErrors()},
      {"queued",queue.queuedCount()},{"sinkCalls",sinkCalls}});
  }
  std::cout<<value::stringify(output)<<'\n';
}
