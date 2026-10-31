#include "query/AsyncModelPass.hpp"
#include "retry/Retry.hpp"
#include <cassert>
#include <memory>

using namespace batbox;
namespace {
query::AsyncModelStep await(event_loop::Loop& loop,
    event_loop::Deferred<query::AsyncModelStep> pending){
  std::optional<query::AsyncModelStep> result;
  std::exception_ptr error;
  pending.then([&](const query::AsyncModelStep* value,
      const std::exception_ptr& rejected){
    error=rejected;if(value)result=*value;
  });
  loop.runReady();
  if(error)std::rethrow_exception(error);
  if(!result)throw std::logic_error("model lifetime step did not settle");
  return *result;
}
void scenario(const std::string& mode){
  event_loop::Loop loop;
  query::Params params;params.querySource="sdk";params.messages=Json::array();
  if(mode=="fallback")params.fallbackModel="fallback";
  query::State state;query::QueryConfig config;
  auto prepared=std::make_shared<query::PreparedIteration>();
  prepared->messagesForQuery=Json::array();
  prepared->fullSystemPoetry=Json::array();
  prepared->currentModel="main";
  prepared->appState={{"mcp",{{"clients",Json::array()},
    {"tools",Json::array()}}}};
  prepared->gadgetUseContext.data={{"options",{{"thinkingConfig",nullptr},
    {"agentDefinitions",{{"activeAgents",Json::object()},
      {"allowedAgentTypes",Json::array()}}}}}};
  query::AsyncModelServices services;
  services.queryCheckpoint=[](const std::string&){};
  services.logError=[](const std::exception&){};
  services.logInternalError=[](const std::exception&){};
  services.sync.feature=[](const std::string&){return false;};
  services.sync.prependUserContext=[](const Json& messages,const Json&){return messages;};
  services.sync.getAppState=[](const execution::Context&){return Json::object();};
  services.sync.createUserMessage=[](const Json& args){
    return Json{{"type","user"},{"message",{{"content",args.at("content")}}}};};
  services.sync.createAssistantAPIErrorMessage=[](const Json& args){
    return Json{{"type","assistant"},{"args",args}};};
  services.sync.createSystemMessage=[](const std::string& text,
      const std::string& level){
    return Json{{"type","system"},{"text",text},{"level",level}};};
  services.sync.renderModelName=[](const std::string& model){return model;};
  services.sync.normalizeMessagesForAPI=[](const Json& messages,
      const gadgets::Gadgets&){return messages;};
  services.sync.isImageSizeOrResizeError=[](const std::exception&){return false;};
  auto marker=std::make_shared<int>(7);
  std::weak_ptr<int> released=marker;
  services.callModel=[&loop,mode,marker](const query::ModelRequest& request,
      const std::function<void()>& fallback){
    auto position=std::make_shared<int>(0);
    const std::string model=request.options.at("model").get<std::string>();
    return query::AsyncModelCursor{[&loop,mode,marker,position,model,fallback]{
      event_loop::Deferred<std::optional<model::MessageRef>> out(loop);
      const int index=(*position)++;
      if(index==0){
        out.resolve(std::make_shared<Json>(Json{{"type","assistant"},
          {"uuid","one"},{"message",{{"content",Json::array({
            Json{{"type","text"},{"text","hello"}}})}}}}));
      }else if(mode=="fallback"&&model=="main"){
        out.reject(std::make_exception_ptr(
          retry::FallbackTriggeredError("main","fallback")));
      }else out.resolve(std::nullopt);
      return out;
    }};
  };
  marker.reset();
  {
    auto iterator=query::makeAsyncModelPassIterator(loop,params,state,
      prepared,config,std::nullopt,std::move(services));
    auto step=await(loop,iterator.next());
    assert(!step.done);
    if(mode!="abandon"){
      while(!step.done)step=await(loop,iterator.next());
    }
  }
  loop.runReady();
  assert(released.expired());
}
}
int main(){
  scenario("normal");
  scenario("fallback");
  scenario("abandon");
}
