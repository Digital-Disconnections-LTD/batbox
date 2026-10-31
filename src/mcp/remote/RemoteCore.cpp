#include "mcp/remote/RemoteClient.hpp"
#include "mcp/remote/EventStream.hpp"
#include "mcp/StdioClient.hpp"
#include "connectors/Abort.hpp"
#include <filesystem>
#include <stdexcept>

namespace batbox::mcp {
RemoteClient::RemoteClient(RemoteConfig config):config_(std::move(config)),endpoint_(config_.url){
  if(config_.name.empty()||config_.url.empty()||!config_.fetch||
      (config_.type!="http"&&config_.type!="sse"&&config_.type!="sse-ide"))
    throw std::invalid_argument("Remote MCP name, URL and supported transport required");
}
RemoteClient::~RemoteClient(){shutdown();}
Json RemoteClient::headers(const std::string& accept){
  Json result={{"User-Agent","Batbox/0.0.1"},{"Accept",accept}};
  if(config_.auth){
    const auto tokens=config_.auth->tokens();const auto& token=value::property(tokens,"access_token");
    if(token.is_string()&&!token.get<std::string>().empty())
      result["Authorization"]="Bearer "+token.get<std::string>();
  }
  if(config_.headers.is_object())for(auto it=config_.headers.begin();it!=config_.headers.end();++it)
    result[it.key()]=it.value();
  std::lock_guard lock(stateMutex_);
  if(!protocol_.empty())result["MCP-Protocol-Version"]=protocol_;
  if(!flightId_.empty())result["Mcp-Session-Id"]=flightId_;
  return result;
}
void RemoteClient::initialize(std::stop_token signal){
  std::lock_guard initialization(initializationMutex_);
  if(connected_&&!expired_)return;
  requestLifetime_=std::stop_source();
  if(listener_.joinable()){listener_.request_stop();listener_.join();}
  closing_=false;expired_=false;
  {std::lock_guard lock(stateMutex_);endpoint_=config_.type=="http"?config_.url:std::string{};
    flightId_.clear();protocol_.clear();lastEventId_.clear();}
  connected_=true;
  try{
    if(config_.type!="http"){
      listener_=std::jthread([this](std::stop_token stop){listen(stop);});
      const auto deadline=std::chrono::steady_clock::now()+config_.connectTimeout;
      std::unique_lock lock(stateMutex_);
      while(endpoint_.empty()&&connected_){
        checkConnectorAbort(signal);
        checkConnectorAbort(config_.lifecycleSignal);
        if(std::chrono::steady_clock::now()>=deadline)
          throw std::runtime_error("MCP SSE endpoint discovery timed out");
        endpointReady_.wait_for(lock,std::chrono::milliseconds(50));
      }
      if(endpoint_.empty())throw std::runtime_error("MCP SSE connection ended before endpoint discovery");
    }
    Json clientCapabilities={{"roots",Json::object()}};
    if(config_.handlers.requestAsync)
      clientCapabilities["elicitation"]={{"form",Json::object()},
        {"url",Json::object()}};
    auto result=request("initialize",{{"protocolVersion","2025-11-25"},
      {"capabilities",std::move(clientCapabilities)},
      {"clientInfo",{{"name","batbox"},{"title","Batbox"},{"version","0.0.1"}}}},
      signal,{},config_.connectTimeout);
    if(!result.is_object()||!value::property(result,"protocolVersion").is_string())
      throw std::runtime_error("MCP initialization returned no protocol version");
    {
      std::lock_guard lock(stateMutex_);protocol_=result.at("protocolVersion").get<std::string>();
      capabilities_=value::property(result,"capabilities");serverInfo_=value::property(result,"serverInfo");
      const auto& instructions=value::property(result,"instructions");
      instructions_=instructions.is_string()?value::slice(instructions.get<std::string>(),0,2048):std::string{};
    }
    send({{"jsonrpc","2.0"},{"method","notifications/initialized"}},signal,config_.requestTimeout);
    if(config_.type=="http")listener_=std::jthread([this](std::stop_token stop){listen(stop);});
  }catch(...){
    connected_=false;if(listener_.joinable())listener_.request_stop();
    failAll(std::current_exception());throw;
  }
}
void RemoteClient::ensureConnected(){
  if(expired_)initialize();
  if(!connected_)throw std::runtime_error("MCP server disconnected: "+config_.name);
}
void RemoteClient::expireFlight(){expired_=true;connected_=false;}
void RemoteClient::shutdown()noexcept{
  if(closing_.exchange(true))return;
  requestLifetime_.request_stop();
  connected_=false;
  if(listener_.joinable()){
    listener_.request_stop();
    if(listener_.get_id()!=std::this_thread::get_id())listener_.join();
  }
  try{
    Json cleanupHeaders;
    try{cleanupHeaders=headers("application/json, text/event-stream");}
    catch(...){cleanupHeaders=config_.headers;}
    std::string flight;
    {std::lock_guard lock(stateMutex_);flight=std::move(flightId_);}
    if(config_.type=="http"&&!flight.empty()){
      http::Request request;request.method="DELETE";request.url=config_.url;
      request.headers=std::move(cleanupHeaders);request.headers["Mcp-Session-Id"]=flight;
      request.timeout=std::chrono::milliseconds(2000);
      (void)config_.cleanupFetch(request);
    }
    failAll(std::make_exception_ptr(std::runtime_error("MCP client closed")));
    endpointReady_.notify_all();
  }catch(...){}
}
Json RemoteClient::capabilities()const{std::lock_guard lock(stateMutex_);return capabilities_;}
Json RemoteClient::serverInfo()const{std::lock_guard lock(stateMutex_);return serverInfo_;}
std::string RemoteClient::instructions()const{std::lock_guard lock(stateMutex_);return instructions_;}
void RemoteClient::closeNotification(){
  if(!connected_.exchange(false)||closing_)return;
  requestLifetime_.request_stop();
  endpointReady_.notify_all();
  failAll(std::make_exception_ptr(std::runtime_error("MCP connection closed")));
  if(config_.handlers.closed)config_.handlers.closed();
}
} // namespace batbox::mcp
