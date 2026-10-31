#include "mcp/manager/IndigenousRuntime.hpp"
#include "application/Async.hpp"
#include "runtime/Platform.hpp"
#include <atomic>
#include <filesystem>
#include <iostream>
#include <thread>

using namespace batbox;
namespace {
void require(bool value,const std::string& message){if(!value)throw std::runtime_error(message);}
template<class T>T await(event_loop::Loop& loop,std::function<event_loop::Deferred<T>()> callback){
  return application::awaitOnOwner<T>(loop.dispatcher(),std::move(callback));
}
mcp::Services gadgetServices(){
  mcp::Services result;result.nowMs=platform::nowMillis;
  result.processResult=[](const Json& value,const std::string&,const std::string&){return value.at("content");};
  result.renderGadgetUseMessage=[](const Json& value,const Json&){return value;};
  result.renderGadgetResultMessage=[](const Json&,const Json& value,const Json&){return value;};
  result.renderGadgetUseProgressMessage=[](const Json& value,const Json&){return value;};
  result.isOutputLineTruncated=[](const std::string&){return false;};
  result.logError=[](const std::string& message){std::cerr<<message<<'\n';};return result;
}
Json helpers(const Json& input){
  const auto kind=input.at("kind").get<std::string>();
  if(kind=="key")return mcp::auth::getServerKey(input.at("name"),input.at("config"));
  if(kind=="scope")return mcp::auth::getScopeFromMetadata(input.value("metadata",Json()));
  if(kind=="redact")return mcp::auth::redactSensitiveUrlParams(input.at("url").get<std::string>());
  if(kind=="normalize"){
    mcp::http::Response response;response.status=input.value("status",200);
    response.body=input.at("body").get<std::string>();response=mcp::auth::normalizeOAuthErrorBody(std::move(response));
    return Json{{"status",response.status},{"body",response.body}};
  }
  throw std::runtime_error("Unknown helper operation");
}
}
int main(int argc,char** argv){
  if(argc==2&&std::string(argv[1])=="helpers"){
    for(std::string line;std::getline(std::cin,line);)try{std::cout<<helpers(Json::parse(line)).dump()<<'\n';}
      catch(const std::exception& error){std::cout<<Json{{"error",error.what()}}.dump()<<'\n';}
    return 0;
  }
  if(argc!=5)throw std::runtime_error("Base URL, state directory, Python and stdio fixture required");
  const std::string base=argv[1];event_loop::Loop loop;
  const std::filesystem::path directory=argv[2];std::filesystem::create_directories(directory);
  std::exception_ptr failure;std::size_t changes=0;std::atomic<int> browserCallbacks=0;
  mcp::manager::IndigenousInputs inputs(loop);inputs.configHome=directory;inputs.projectRoot=directory;
  inputs.explicitConfigurations=true;
  inputs.configurations={{"private",{{"type","http"},{"url",base+"/mcp"}}},
    {"legacy",{{"type","sse"},{"url",base+"/sse"}}},
    {"local",{{"type","stdio"},{"command",argv[3]},{"args",Json::array({argv[4]})}}}};
  inputs.gadgetServices=gadgetServices();inputs.changed=[&](const auto&){++changes;};
  inputs.authorizationUrl=[&](const std::string&,const std::string& url){
    ++browserCallbacks;
    application::runInWorker<event_loop::Unit>(loop,[url]{
      mcp::http::Request request;request.url=url;request.timeout=std::chrono::seconds(5);
      require(mcp::http::fetch(request).status==200,"OAuth callback did not finish");
      return event_loop::Unit{};
    }).then([&](const event_loop::Unit*,const std::exception_ptr& error){if(error)failure=error;});
  };
  auto runtime=mcp::manager::IndigenousRuntime::create(std::move(inputs));
  const auto type=[&](const std::string& name){
    return application::onOwnerAndWait<std::string>(loop.dispatcher(),[&]{
      for(const auto& server:runtime->manager()->snapshot())if(server.name==name)return server.type;
      return std::string{};
    });
  };
  const auto call=[&](const std::string& server,const std::string& method,Json params){
    return await<Json>(loop,[&,server,method,params=std::move(params)]{
      return runtime->manager()->invoke(server,method,params);});
  };
  application::runInWorker<event_loop::Unit>(loop,[&]{
    await<event_loop::Unit>(loop,[&]{return runtime->start();});
    require(type("private")=="needs-auth","HTTP server should request OAuth login");
    require(type("legacy")=="connected"&&type("local")=="connected","SSE/stdio startup failed");
    require(call("local","tools/call",{{"name","echo"},{"arguments",{{"text","stdio"}}}})
      .at("content").at(0).at("text")=="stdio","stdio invocation failed");
    require(call("legacy","tools/call",{{"name","echo"},{"arguments",{{"text","legacy"}}}})
      .at("content").at(0).at("text")=="legacy","legacy SSE invocation failed");
    await<event_loop::Unit>(loop,[&]{return runtime->authenticate("private",{},true);});
    for(int attempt=0;attempt<200&&type("private")!="connected";++attempt)
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    require(type("private")=="connected","OAuth server did not connect after login");
    require(browserCallbacks==1,"OAuth login opened more than one authorization flow");
    require(call("private","resources/read",{{"uri","memory://fixture"}})
      .at("contents").at(0).at("text")=="resource content","MCP resource unavailable");
    require(call("private","prompts/get",{{"name","greet"},{"arguments",{{"name","reader"}}}})
      .at("messages").at(0).at("content").at("text")=="Hello reader","MCP prompt unavailable");
    auto connection=application::onOwnerAndWait<std::shared_ptr<mcp::Connection>>(loop.dispatcher(),[&]{
      for(const auto& server:runtime->manager()->snapshot())if(server.name=="private")return server.connection;
      return std::shared_ptr<mcp::Connection>{};
    });
    int progress=0;
    const auto streamed=connection->callGadget("echo",{{"text","streamed"}},{},[&](const Json&){++progress;});
    require(streamed.content.at(0).at("text")=="streamed"&&progress==1,"POST SSE result/progress failed");
    const auto counts=application::onOwnerAndWait<Json>(loop.dispatcher(),[&]{
      auto servers=runtime->manager()->snapshot();
      for(const auto& server:servers)if(server.name=="private")return Json{{"tools",server.gadgets.size()},
        {"commands",server.commands.size()},{"resources",server.resources.size()}};
      return Json();
    });
    require(counts==Json{{"tools",2},{"commands",1},{"resources",1}},"MCP discovery/pagination incomplete");
    // Explicit expiration must invalidate the flight; the following request
    // reinitializes rather than replaying a side-effecting request silently.
    try{call("private","tools/call",{{"name","expire"},{"arguments",Json::object()}});
      throw std::runtime_error("Expired session was accepted");}
    catch(const mcp::HttpStatusError& error){require(error.status==404,"Wrong session error");}
    require(call("private","tools/call",{{"name","echo"},{"arguments",{{"text","renewed"}}}})
      .at("content").at(0).at("text")=="renewed","Session recovery failed");
    await<event_loop::Unit>(loop,[&]{return runtime->manager()->toggle("legacy");});
    for(int attempt=0;attempt<200&&type("legacy")!="disabled";++attempt)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    require(type("legacy")=="disabled","Disable did not settle");
    await<event_loop::Unit>(loop,[&]{return runtime->manager()->toggle("legacy");});
    for(int attempt=0;attempt<200&&type("legacy")!="connected";++attempt)std::this_thread::sleep_for(std::chrono::milliseconds(10));
    require(type("legacy")=="connected","Enable did not reconnect");
    await<event_loop::Unit>(loop,[&]{return runtime->shutdown();});
    // Recreating the client exercises disk persistence, proactive refresh and
    // reload of enable/disable state without another interactive login.
    runtime=application::onOwnerAndWait<std::shared_ptr<mcp::manager::IndigenousRuntime>>(loop.dispatcher(),[&]{
      mcp::manager::IndigenousInputs next(loop);next.configHome=directory;next.projectRoot=directory;
      next.explicitConfigurations=true;next.configurations={{"private",{{"type","http"},{"url",base+"/mcp"}}}};
      next.gadgetServices=gadgetServices();return mcp::manager::IndigenousRuntime::create(std::move(next));
    });
    await<event_loop::Unit>(loop,[&]{return runtime->start();});
    require(type("private")=="connected","Persisted OAuth login did not reconnect");
    require(call("private","tools/call",{{"name","echo"},{"arguments",{{"text","persistent"}}}})
      .at("content").at(0).at("text")=="persistent","Persisted token call failed");
    await<event_loop::Unit>(loop,[&]{return runtime->logout("private");});
    await<event_loop::Unit>(loop,[&]{return runtime->shutdown();});
    return event_loop::Unit{};
  }).then([&](const event_loop::Unit*,const std::exception_ptr& error){
    if(error)failure=error;
    runtime->shutdown().then([&](const event_loop::Unit*,const std::exception_ptr& shutdownError){
      if(shutdownError&&!failure)failure=shutdownError;loop.stop();});
  });
  auto lifetime=loop.hold();loop.run();lifetime.release();
  if(failure)std::rethrow_exception(failure);
  require(changes>3,"Connection updates were not published");
  std::cout<<"Native MCP OAuth, persistence, refresh, pagination, resources, prompts, stdio, HTTP/SSE and shutdown completed\n";
}
