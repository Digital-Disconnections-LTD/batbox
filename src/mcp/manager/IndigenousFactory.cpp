#include "mcp/manager/IndigenousRuntime.hpp"
#include "mcp/remote/RemoteClient.hpp"
#include "mcp/StdioClient.hpp"
#include "application/Async.hpp"
#include "process/ExecFileNoThrow.hpp"
#include "runtime/IndigenousPath.hpp"
#include "connectors/Abort.hpp"
#include <cstdlib>
#include <mutex>
#include <set>

namespace batbox::mcp::manager {
namespace {
std::string text(const Json& value,const char* key){const auto& item=value::property(value,key);
  return item.is_string()?item.get<std::string>():std::string{};}
std::chrono::milliseconds timeout(const char* key,std::int64_t fallback){
  const auto* configured=std::getenv(key);if(!configured)return std::chrono::milliseconds(fallback);
  try{std::size_t used=0;const auto value=std::stoll(configured,&used);
    if(used==std::char_traits<char>::length(configured)&&value>0)return std::chrono::milliseconds(value);
  }catch(...){}
  return std::chrono::milliseconds(fallback);
}
Json listPages(const std::shared_ptr<Connection>& client,const std::string& method,
    const char* key,std::stop_token signal){
  Json result=Json::array(),params=Json::object();std::set<std::string> cursors;
  for(;;){
    checkConnectorAbort(signal);const auto page=client->request(method,params,signal);
    const auto& values=value::property(page,key);
    if(!values.is_array())throw std::runtime_error("MCP "+method+" returned no "+key+" array");
    for(const auto& value:values)result.push_back(value);
    const auto cursor=text(page,"nextCursor");if(cursor.empty())return result;
    if(!cursors.insert(cursor).second)throw std::runtime_error("MCP pagination repeated its cursor");
    params["cursor"]=cursor;
  }
}
struct ConnectionOwner {
  std::shared_ptr<StdioClient> stdio;
  std::shared_ptr<RemoteClient> remote;
  std::once_flag closed;
  void close(){std::call_once(closed,[this]{if(stdio)stdio->shutdown();if(remote)remote->shutdown();});}
  ~ConnectionOwner(){close();}
};
}
event_loop::Deferred<ServerResult> IndigenousRuntime::connectBatServer(
    const std::string& name,const Json& config){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Agent MCP connection requires application owner");
  if(stopped_)throw std::runtime_error("MCP runtime is shutting down");
  if(name.empty()||!config.is_object())
    throw std::invalid_argument("Agent MCP server name and configuration required");
  const auto weak=weak_from_this();const auto owner=inputs_.owner.dispatcher();
  ClientHandlers handlers;
  handlers.request=[weak,owner,name](const Json& request)->Json{
    return application::onOwnerAndWait<Json>(owner,[weak,name,request]{
      auto self=weak.lock();if(!self)throw std::runtime_error("MCP runtime closed");
      if(self->inputs_.serverRequest)return self->inputs_.serverRequest(name,request);
      const auto& method=value::property(request,"method");
      if(method=="roots/list")return Json{{"roots",Json::array({{{"uri","file://"+
        runtime::pathToUtf8(self->inputs_.projectRoot)},
        {"name",runtime::pathToUtf8(self->inputs_.projectRoot.filename())}}})}};
      if(method=="ping")return Json::object();
      if(method=="elicitation/create")return Json{{"action","cancel"}};
      throw std::runtime_error("MCP client method unavailable");
    });
  };
  handlers.requestAsync=[weak,owner,name](const Json& request,
      std::stop_token signal,
      std::function<void(Json,std::exception_ptr)> complete){
    owner.postMission([weak,name,request,signal,complete=std::move(complete)]{
      try{
        auto self=weak.lock();
        if(!self){complete(Json{{"action","cancel"}},{});return;}
        self->requestElicitation(name,request,signal).then(
          [complete](const Json* value,const std::exception_ptr& error){
            if(error)complete(Json(),error);
            else complete(value?*value:Json{{"action","cancel"}},{});
          });
      }catch(...){complete(Json(),std::current_exception());}
    });
  };
  handlers.notification=[weak,owner,name](const Json& value){
    owner.postMission([weak,name,value]{if(auto self=weak.lock())
      if(self->inputs_.notification)self->inputs_.notification(name,value);});
  };
  handlers.closed=[weak,owner,name]{owner.postMission([weak,name]{
    if(auto self=weak.lock())self->inputs_.diagnostic(name,"Agent MCP server closed");});};
  handlers.needsAuth=[weak,owner,name]{owner.postMission([weak,name]{
    if(auto self=weak.lock())self->inputs_.diagnostic(name,"Agent MCP authorization required");});};
  return connect(name,config,std::move(handlers));
}
event_loop::Deferred<ServerResult> IndigenousRuntime::connect(const std::string& name,
    const Json& raw,ClientHandlers handlers){
  auto self=shared_from_this();const auto signal=lifetime_.get_token();
  const auto services=inputs_.gadgetServices;const auto projectRoot=inputs_.projectRoot;
  return application::runInWorker<ServerResult>(inputs_.owner,[self,name,raw,handlers=std::move(handlers),signal,services,projectRoot]{
    ServerResult result;result.name=name;result.config=raw;
    auto owned=std::make_shared<ConnectionOwner>();
    try{
      checkConnectorAbort(signal);const auto config=expandServerConfiguration(raw);
      if(!config.is_object())throw std::runtime_error("MCP server configuration must be an object");
      auto type=text(config,"type");if(type.empty())type=text(config,"url").empty()?"stdio":"http";
      const bool skip=config.value("skipPrefix",false);
      if(type=="stdio"){
        const auto executable=text(config,"command");
        if(executable.empty())throw std::runtime_error("MCP server command required");
        std::vector<std::string> command{executable};
        for(const auto& value:config.value("args",Json::array())){
          if(!value.is_string())throw std::runtime_error("MCP arguments must be strings");
          command.push_back(value.get<std::string>());
        }
        std::map<std::string,std::string> environment;
        const auto overrides=config.value("env",Json::object());
        if(!overrides.is_object())throw std::runtime_error("MCP environment must be an object");
        for(auto it=overrides.begin();it!=overrides.end();++it){
          if(!it.value().is_string())throw std::runtime_error("MCP environment values must be strings");
          environment[it.key()]=it.value().get<std::string>();
        }
        owned->stdio=std::make_shared<StdioClient>(command,name,timeout("MCP_TOOL_TIMEOUT",100000000),
          environment,timeout("MCP_TIMEOUT",30000),std::chrono::milliseconds(60000),handlers,signal);
        result.connection=makeStdioConnection(owned->stdio,name,skip);
      }else if(type=="http"||type=="sse"||type=="sse-ide"){
        RemoteConfig remote;remote.name=name;remote.url=text(config,"url");remote.type=type;
        remote.originalCwd=runtime::pathToUtf8(projectRoot);
        remote.headers=config.value("headers",Json::object());
        if(!remote.headers.is_object())throw std::runtime_error("MCP HTTP headers must be an object");
        for(const auto& value:remote.headers)if(!value.is_string())throw std::runtime_error("MCP HTTP header values must be strings");
        const auto helper=text(config,"headersHelper");
        if(!helper.empty()){
          try{
            const auto execute=process::indigenousExeca(self->inputs_.owner);
            const auto generated=application::awaitOnOwner<process::ExecaResult>(self->inputs_.owner.dispatcher(),
              [execute,helper,name,config,signal,cwd=remote.originalCwd]{
                process::ExecaRequest request;request.file=helper;request.options.shell=true;
                request.options.cwd=cwd;request.options.timeout=10000;request.options.abortSignal=signal;
                request.options.env=std::map<std::string,std::string>{{"BATBOX_MCP_SERVER_NAME",name},
                  {"BATBOX_MCP_SERVER_URL",text(config,"url")}};
                return execute(std::move(request));
              });
            if(generated.failed||generated.exitCode.value_or(1)!=0||value::trim(generated.stdoutText).empty())
              throw std::runtime_error("MCP headersHelper did not return a valid value");
            const auto dynamic=Json::parse(generated.stdoutText);
            if(!dynamic.is_object())throw std::runtime_error("MCP headersHelper must return a JSON object");
            for(auto it=dynamic.begin();it!=dynamic.end();++it){
              if(!it.value().is_string())throw std::runtime_error("MCP headersHelper values must be strings");
              remote.headers[it.key()]=it.value();
            }
          }catch(const std::exception&){self->inputs_.diagnostic(name,"MCP headersHelper failed; using static headers");}
        }
        auto authentication=self->authenticationServices(name);
        {
          std::lock_guard lock(*authentication.storageMutex);
          if(auth::hasMcpDiscoveryButNoToken(name,config,authentication.storage)){
            result.type="needs-auth";return result;
          }
        }
        remote.auth=std::make_shared<auth::Provider>(name,config,std::move(authentication));
        remote.fetch=self->auth_.services.fetch;remote.cleanupFetch=self->inputs_.fetch;
        remote.connectTimeout=timeout("MCP_TIMEOUT",30000);
        remote.gadgetTimeout=timeout("MCP_TOOL_TIMEOUT",100000000);remote.handlers=handlers;
        remote.lifecycleSignal=signal;
        owned->remote=std::make_shared<RemoteClient>(std::move(remote));owned->remote->initialize(signal);
        result.connection=makeRemoteConnection(owned->remote,name,type,skip);
      }else throw std::runtime_error("Unsupported MCP transport: "+type);
      const auto owner=self->inputs_.owner.dispatcher();
      result.connection->postToOwner=[owner](std::function<void()> action){owner.postMission(std::move(action));};
      result.owner=owned;result.close=[owned]{owned->close();};result.type="connected";
      // The Indigenous SDK adapter handles pagination; gadget construction consumes
      // one ListGadgets response.
      const auto wire=result.connection->request;
      auto listing=std::make_shared<Connection>();listing->request=wire;
      result.connection->listGadgets=[listing](std::stop_token stop){
        return Json{{"tools",listPages(listing,"tools/list","tools",stop)}};};
      return self->discover(std::move(result),"all",signal,services);
    }catch(const UnauthorizedError& error){result.type="needs-auth";result.error=error.what();}
    catch(const std::exception& error){result.type="failed";result.error=error.what();}
    catch(...){result.type="failed";result.error="MCP connection failed";}
    owned->close();result.connection.reset();result.owner.reset();result.close={};
    return result;
  });
}
ServerResult IndigenousRuntime::discover(ServerResult result,const std::string& kind,
    std::stop_token signal,const Services& services){
  if(!result.connection)return result;
  if(kind=="all"||kind=="tools")result.gadgets=fetchGadgetsForClient(result.connection,services);
  const auto list=[&](const std::string& capability,const std::string& method,const char* key){
    try{
      if(!result.connection->capabilities.contains(capability))return Json::array();
      return listPages(result.connection,method,key,signal);
    }catch(const std::exception&){inputs_.diagnostic(result.name,"MCP "+method+" failed");return Json::array();}
  };
  if(kind=="all"||kind=="resources"){
    result.resources=list("resources","resources/list","resources");
    for(auto& resource:result.resources)resource["server"]=result.name;
  }
  if(kind=="all"||kind=="commands"){
    const auto poetryEntries=recursivelySanitizeUnicode(list("prompts","prompts/list","prompts"));
    result.commands=Json::array();
    for(const auto& poetry:poetryEntries){
      const auto name=text(poetry,"name");if(name.empty())continue;
      Json arguments=Json::array();for(const auto& item:poetry.value("arguments",Json::array()))arguments.push_back(text(item,"name"));
      result.commands.push_back({{"type","prompt"},{"name","mcp__"+normalizeNameForMCP(result.name,"")+"__"+name},
        {"description",text(poetry,"description")},{"hasUserSpecifiedDescription",!text(poetry,"description").empty()},
        {"contentLength",0},{"isHidden",false},{"isMcp",true},{"source","mcp"},
        {"progressMessage","running"},{"argNames",arguments},{"serverName",result.name},{"promptName",name},
        {"userFacingName",result.name+":"+name+" (MCP)"}});
    }
  }
  return result;
}
event_loop::Deferred<ServerResult> IndigenousRuntime::refresh(const ServerResult& current,const std::string& kind){
  auto self=shared_from_this();const auto signal=lifetime_.get_token();const auto services=inputs_.gadgetServices;
  return application::runInWorker<ServerResult>(inputs_.owner,[self,current,kind,signal,services]{
    return self->discover(current,kind,signal,services);});
}
} // namespace batbox::mcp::manager
