#include "diagnostics/ErrorLogSink.hpp"
#include "runtime/JsonValue.hpp"
#include <deque>
#include <iostream>
#include <map>
#include <set>

int main(){
  using namespace batbox;
  using namespace batbox::diagnostics;
  const auto input=value::parse(std::string(std::istreambuf_iterator<char>(std::cin),{}));
  Json writes=Json::array(),debug=Json::array(),mkdirs=Json::array(),out=Json::array();
  std::set<std::string> failFirst;
  for(const auto& path:input.value("failFirst",Json::array()))
    failFirst.insert(path.get<std::string>());
  std::map<int,std::function<void()>> timers;
  std::deque<std::function<void()>> immediates;
  int nextTimer=1,attachments=0,cleanups=0;
  event_loop::Loop loop;
  ErrorLogServices services;
  services.loop=&loop;
  services.errorsDirectory=[]{return "/batbox/errors";};
  services.mcpLogsDirectory=[](const std::string& server){return "/batbox/mcp/"+server;};
  services.timestampISO=[]{return "2026-03-31T12:34:56.000Z";};
  services.flightId=[]{return "session-1";};
  services.cwd=[]{return "/work";};
  services.version=[]{return "0.0.1";};
  services.isInternalUser=[&]{return input.value("internal",false);};
  services.internalUserTypeLabel=[]{return "ant";};
  services.logForDebugging=[&](const std::string& message,const std::string& level){
    debug.push_back(Json::array({level,message}));};
  services.appendFileSync=[&](const std::string& path,const std::string& value){
    if(failFirst.erase(path))throw std::runtime_error("first append failed");
    writes.push_back(Json::array({path,value}));};
  services.mkdirSync=[&](const std::string& path){mkdirs.push_back(path);};
  services.registerCleanup=[&](std::function<void()>){++cleanups;};
  services.attachErrorLogSink=[&](ErrorLogCallbacks){++attachments;};
  services.makeBufferedWriterServices=[&](std::function<void(const std::string&)> write){
    BufferedWriterServices options;
    options.writeFn=std::move(write);
    options.setTimeout=[&](double,std::function<void()> callback){
      const int id=nextTimer++;timers[id]=std::move(callback);
      return std::function<void()>([&,id]{timers.erase(id);});
    };
    options.setImmediate=[&](std::function<void()> callback){
      immediates.push_back(std::move(callback));};
    return options;
  };
  ErrorLogSink sink(std::move(services),"2026-03-31T12:34:56.000Z");
  auto makeError=[](const Json& spec){
    ErrorValue error;
    error.name=spec.value("name",std::string("Error"));
    error.message=spec.value("message",std::string{});
    if(spec.contains("stack"))error.stack=spec["stack"].get<std::string>();
    error.isHttpClientError=spec.value("isAxios",false);
    if(spec.contains("requestUrl"))error.requestUrl=spec["requestUrl"].get<std::string>();
    if(spec.contains("responseStatus"))error.responseStatus=spec["responseStatus"].get<int>();
    if(spec.contains("responseData"))error.responseData=spec["responseData"];
    return error;
  };
  for(const auto& action:input["actions"]){
    Json value=nullptr;
    const auto kind=action["kind"].get<std::string>();
    if(kind=="errorsPath")value=sink.getErrorsPath();
    if(kind=="mcpPath")value=sink.getMCPLogsPath(action["server"].get<std::string>());
    if(kind=="error")sink.logError(makeError(action["error"]));
    if(kind=="mcpError")sink.logMCPError(action["server"].get<std::string>(),
      makeError(action["error"]));
    if(kind=="mcpUnknown")sink.logMCPUnknownError(
      action["server"].get<std::string>(),action["value"]);
    if(kind=="mcpDebug")sink.logMCPDebug(action["server"].get<std::string>(),
      action["message"].get<std::string>());
    if(kind=="flush")sink.flushLogWritersForTesting();
    if(kind=="clear")sink.clearLogWritersForTesting();
    if(kind=="init")sink.initializeErrorLogSink();
    if(kind=="immediate"&&!immediates.empty()){
      auto callback=std::move(immediates.front());immediates.pop_front();callback();
    }
    out.push_back({{"value",value},{"writes",writes},{"debug",debug},
      {"mkdirs",mkdirs},{"attachments",attachments},{"cleanups",cleanups},
      {"timers",timers.size()},{"immediates",immediates.size()}});
  }
  std::cout<<value::stringify(out)<<'\n';
}
