#include "mcp/manager/IndigenousRuntime.hpp"
#include "auth/oauth_listener/IndigenousServer.hpp"
#include "application/Async.hpp"
#include "runtime/Platform.hpp"
#include <cstdlib>
#include <uv.h>

namespace batbox::mcp::manager {
namespace {
bool available(int port){
  uv_loop_t loop;if(uv_loop_init(&loop))return false;
  uv_tcp_t server;int status=uv_tcp_init(&loop,&server);
  if(!status){
    sockaddr_in address;status=uv_ip4_addr("127.0.0.1",port,&address);
    if(!status)status=uv_tcp_bind(&server,reinterpret_cast<const sockaddr*>(&address),0);
    if(!status)status=uv_listen(reinterpret_cast<uv_stream_t*>(&server),1,[](uv_stream_t*,int){});
    uv_close(reinterpret_cast<uv_handle_t*>(&server),nullptr);uv_run(&loop,UV_RUN_DEFAULT);
  }
  uv_loop_close(&loop);return status==0;
}
struct FlowLifetime {
  std::stop_source stopped;
  std::stop_callback<std::function<void()>> caller,lifetime;
  FlowLifetime(std::stop_token first,std::stop_token second):
    caller(first,std::function<void()>([this]{stopped.request_stop();})),
    lifetime(second,std::function<void()>([this]{stopped.request_stop();})){}
};
}
event_loop::Deferred<event_loop::Unit> IndigenousRuntime::authenticate(const std::string& name,
    std::stop_token signal,bool skipBrowserOpen){
  if(!inputs_.owner.isOwnerThread())throw std::logic_error("MCP authentication requires owner");
  if(stopped_)throw std::runtime_error("MCP runtime closed");
  if(!configurations_.contains(name))throw std::runtime_error("MCP server not found: "+name);
  auto linked=std::make_shared<FlowLifetime>(signal,lifetime_.get_token());
  auth::FlowInputs flow(inputs_.owner);flow.name=name;flow.config=expandServerConfiguration(configurations_.at(name));
  if(!value::property(flow.config,"url").is_string())throw std::runtime_error("MCP OAuth requires an HTTP or SSE server");
  flow.services=authenticationServices(name);flow.signal=linked->stopped.get_token();flow.skipBrowserOpen=skipBrowserOpen;
  flow.createServer=[this]{return ::batbox::auth::oauth_listener::makeIndigenousCallbackServer(inputs_.owner);};
  flow.findPort=[]{
    auth::PortServices ports;ports.available=available;ports.random=platform::randomUnitInterval;
    if(const auto* configured=std::getenv("MCP_OAUTH_CALLBACK_PORT"))ports.configured=configured;
#if defined(_WIN32)
    ports.windows=true;
#endif
    return auth::findAvailablePort(ports);
  };
  if(inputs_.waitingForCallback)flow.onWaitingForCallback=[waiting=inputs_.waitingForCallback,name](auto callback){waiting(name,std::move(callback));};
  ++authJobs_;auto self=shared_from_this();event_loop::Deferred<event_loop::Unit> output(inputs_.owner);
  auth::performMCPOAuthFlow(std::move(flow)).then([self,output,linked,name](const event_loop::Unit*,const std::exception_ptr& error){
    if(error){--self->authJobs_;self->finishShutdown();output.reject(error);return;}
    if(self->stopped_){--self->authJobs_;self->finishShutdown();output.reject(std::make_exception_ptr(auth::AuthenticationCancelledError()));return;}
    self->manager_->reconnect(name).then([self,output,linked](const ServerResult* server,const std::exception_ptr& reconnectError){
      --self->authJobs_;self->finishShutdown();
      if(reconnectError)output.reject(reconnectError);
      else if(!server||server->type!="connected")output.reject(std::make_exception_ptr(std::runtime_error(
        server&&!server->error.empty()?server->error:"MCP authorization completed but connection failed")));
      else output.resolve({});
    });
  });
  return output;
}
event_loop::Deferred<event_loop::Unit> IndigenousRuntime::logout(const std::string& name){
  if(!configurations_.contains(name))throw std::runtime_error("MCP server not found: "+name);
  auto self=shared_from_this();const auto config=expandServerConfiguration(configurations_.at(name));
  ++authJobs_;
  auto result=application::runInWorker<event_loop::Unit>(inputs_.owner,[self,name,config]{
    auth::revokeServerTokens(name,config,self->auth_.services);
    if(self->auth_.services.clearStorageStash)self->auth_.services.clearStorageStash();
    return event_loop::Unit{};
  });
  result.then([self,name](const event_loop::Unit*,const std::exception_ptr& error){
    --self->authJobs_;self->finishShutdown();
    if(!error&&!self->stopped_)self->manager_->reconnect(name);
  });
  return result;
}
} // namespace batbox::mcp::manager
