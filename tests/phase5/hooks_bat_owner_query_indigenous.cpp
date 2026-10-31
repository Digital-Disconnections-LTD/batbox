#include "instructions/hooks/runtime/BatOwnerQueryBinding.hpp"
#include <cassert>
#include <iostream>

using namespace batbox;
namespace hooks=batbox::instructions::hooks::runtime;

namespace {
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> output(loop);output.resolve(std::move(value));
  return output;
}
struct Fixture {
  event_loop::Loop loop;
  hooks::BatHookServices bat;
  int contextCalls=0,serviceCalls=0,modelCalls=0,completed=0;
  Fixture(){
    hooks::BatOwnerQueryBindings bindings;
    bindings.buildContext=[this](const hooks::BatQueryRequest& call){
      ++contextCalls;
      assert(call.batId=="hook-agent-1"&&call.model=="selected");
      assert(call.gadgets.at(0).at("name")=="Read");
      execution::Context context;
      context.stop=call.signal;
      context.data={{"agentId",call.batId},{"options",
        {{"mainLoopModel",call.model},{"isNonInteractiveSession",true},
         {"thinkingConfig",{{"type","disabled"}}},
         {"agentDefinitions",{{"activeAgents",Json::object()},
           {"allowedAgentTypes",Json::array()}}}}}};
      return context;
    };
    bindings.buildServices=[this](const hooks::BatQueryRequest& call){
      ++serviceCalls;
      assert(call.getAppState().at("toolPermissionContext").at("mode")=="dontAsk");
      query::AsyncEngineServices services;
      services.config.getFlightId=[](){return std::string("session");};
      services.config.statsigFeatureGate=[](const std::string&){return false;};
      services.setup.queryCheckpoint=[](const std::string&){};
      services.setup.headlessProfilerCheckpoint=[](const std::string&){};
      services.setup.sync.feature=[](const std::string&){return false;};
      services.setup.sync.uuid=[](){return std::string("chain");};
      services.setup.sync.getMessagesAfterCompactBoundary=[](const Json& value){return value;};
      services.setup.applyGadgetResultBudget=[this](const Json& messages,
        const std::shared_ptr<gadget_results::ContentReplacementState>&,
        const std::function<void(const Json&)>&,const std::vector<std::string>&){
        return ready(loop,messages);};
      services.setup.microcompact=[this](const Json& messages,
        const execution::Context&,const std::string&){
        return ready(loop,Json{{"messages",messages}});};
      services.setup.sync.appendSystemContext=[](const Json& poetry,const Json&){return poetry;};
      services.setup.autocompact=[this](const Json&,const execution::Context&,
        const Json&,const std::string&,const Json&,double){
        return ready(loop,Json{{"compactionResult",nullptr}});};
      services.setup.sync.getAppState=[getter=call.getAppState](
        const execution::Context&){return getter();};
      services.setup.sync.getRuntimeMainLoopModel=[](const Json&){
        return std::string("selected");};
      services.setup.sync.tokenCountWithEstimation=[](const Json&){return 0.0;};
      services.setup.sync.calculateTokenWarningState=[](double,const Json&){
        return Json{{"isAtBlockingLimit",false}};};
      services.model.queryCheckpoint=[](const std::string&){};
      services.model.logError=[](const std::exception& error){
        std::cerr<<"model recovery: "<<error.what()<<'\n';};
      services.model.logInternalError=[](const std::exception& error){
        std::cerr<<"model internal: "<<error.what()<<'\n';};
      services.model.sync.feature=[](const std::string&){return false;};
      services.model.sync.prependUserContext=[](const Json& messages,const Json&){
        return messages;};
      services.model.sync.getAppState=services.setup.sync.getAppState;
      services.model.sync.createUserMessage=[](const Json& args){
        return Json{{"type","user"},{"args",args}};};
      services.model.callModel=[this](const query::ModelRequest& request,
        const std::function<void()>&){
        ++modelCalls;
        assert(request.options.at("model")=="selected");
        auto index=std::make_shared<int>(0);
        return query::AsyncModelCursor{[this,index]{
          if((*index)++==0){
            auto message=std::make_shared<Json>(Json{{"type","assistant"},
              {"message",{{"content",Json::array({Json{{"type","text"},
                {"text","done"}}})}}}});
            return ready(loop,std::optional<model::MessageRef>(message));
          }
          return ready(loop,std::optional<model::MessageRef>{});
        },{}};
      };
      services.startRelevantMemoryPrefetch=[](const Json&,const execution::Context&){
        return query::MemoryPrefetch{};};
      services.executePostSamplingHooks=[](const Json&,const query::Params&,
        const execution::Context&){};
      services.completeWithoutGadgets=[this](const query::Params&,const query::State&,
        std::shared_ptr<query::PreparedIteration>,
        std::shared_ptr<query::ModelPassResult>,std::optional<double>&,
        query::BudgetTracker*){
        ++completed;
        return query::AsyncDecisionCursor{[this]{
          auto decision=std::make_shared<query::CompletionDecision>();
          decision->terminal=Json{{"reason","completed"}};
          return ready(loop,query::AsyncDecisionStep{true,{},decision});
        },{}};
      };
      return services;
    };
    hooks::bindBatOwnerQuery(loop,bat,std::move(bindings));
  }
  hooks::BatMessageStream start(){
    hooks::BatQueryRequest request;
    request.messages=Json::array({Json{{"type","user"}}});
    request.systemPoetry=Json::array({"system"});
    request.gadgets=Json::array({Json{{"name","Read"}}});
    request.batId="hook-agent-1";request.model="selected";
    request.getAppState=[](){return Json{{"toolPermissionContext",
      {{"mode","dontAsk"}}}};};
    return bat.query(std::move(request));
  }
  hooks::GeneratorStep<Json> wait(
    event_loop::Deferred<hooks::GeneratorStep<Json>> output){
    std::optional<hooks::GeneratorStep<Json>> step;
    std::exception_ptr failure;
    output.then([&](const hooks::GeneratorStep<Json>* value,
      const std::exception_ptr& error){failure=error;if(value)step=*value;});
    loop.runReady();
    if(failure)std::rethrow_exception(failure);
    assert(step);return *step;
  }
};
}
int main(){
  Fixture fixture;
  auto stream=fixture.start();
  auto start=fixture.wait(stream.next());
  assert(!start.done&&start.value->at("type")=="stream_request_start");
  auto assistant=fixture.wait(stream.next());
  assert(!assistant.done&&assistant.value->at("type")=="assistant");
  auto done=fixture.wait(stream.next());
  assert(done.done&&fixture.modelCalls==1&&fixture.completed==1);
  assert(fixture.contextCalls==1&&fixture.serviceCalls==1);
  auto second=fixture.start();
  assert(!fixture.wait(second.next()).done);
  auto closed=fixture.wait(second.close());
  assert(closed.done);
  std::cout<<"agent query binding passes owner context, model and iterator close\n";
}
