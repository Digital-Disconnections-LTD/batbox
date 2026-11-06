#include "query/OwnerPreflight.hpp"
#include "query/OwnerPromiseAll.hpp"
#include "model/ModelWrappers.hpp"
#include "runtime/JsonValue.hpp"
#include "transport/Errors.hpp"
#include <stdexcept>

namespace batbox::query {
namespace {
struct Run:std::enable_shared_from_this<Run> {
  event_loop::Loop& loop;
  std::shared_ptr<const model::PreflightInputs> held;
  const model::PreflightInputs& input;
  model::PreflightServices services;
  OwnerPreflightAwaits awaits;
  std::stop_token stop;
  event_loop::Deferred<model::PreflightState> result;
  model::PreflightState seed;
  std::string modelName;
  Run(event_loop::Loop& owner,std::shared_ptr<const model::PreflightInputs> request,
      model::PreflightServices imports,OwnerPreflightAwaits asyncImports,
      std::stop_token signal):loop(owner),held(std::move(request)),input(*held),
      services(std::move(imports)),awaits(std::move(asyncImports)),
      stop(signal),result(owner){}
  void checkStop(){
    if(stop.stop_requested())throw transport::APIUserAbortError();
  }
  void fail(std::exception_ptr error){result.reject(error);}
  void begin(){
    try{
      checkStop();
      const Json& modelValue=value::property(input.options,"model");
      modelName=modelValue.is_string()?modelValue.get<std::string>():std::string{};
      resolveModel();
    }catch(...){fail(std::current_exception());}
  }
  void resolveModel(){
    seed.previousRequestId=model::getPreviousRequestIdFromMessages(input.messages);
    if(!services.getAPIProvider)
      throw std::invalid_argument("API provider import is required");
    seed.providerAtStart=services.getAPIProvider();
    if(seed.providerAtStart=="bedrock"&&
        modelName.find("application-inference-profile")!=std::string::npos){
      if(!awaits.inferenceProfileBackingModel)
        throw std::invalid_argument("Awaited inference profile import is required");
      auto pending=awaits.inferenceProfileBackingModel(modelName);
      if(!loop.dispatcher().sameLoop(pending.dispatcher()))
        throw std::invalid_argument("Inference profile await crossed event loops");
      auto self=shared_from_this();
      pending.then([self](const std::optional<std::string>* resolved,
          const std::exception_ptr& error){
        if(error){self->fail(error);return;}
        try{
          self->checkStop();
          if(!resolved)throw std::logic_error("Inference profile resolved without value");
          self->seed.resolvedModel=resolved->value_or(self->modelName);
          self->finish();
        }catch(...){self->fail(std::current_exception());}
      });
      return;
    }
    seed.resolvedModel=modelName;
    finish();
  }
  void finish(){
    checkStop();
    if(awaits.remainingSegment){
      result.resolve(awaits.remainingSegment(input,services,std::move(seed)));
      return;
    }
    seed=model::prepareQueryModelUntilGadgetSearch(input,services,std::move(seed));
    if(!awaits.gadgetSearchEnabled)
      throw std::invalid_argument("Awaited tool-search import is required");
    auto pending=awaits.gadgetSearchEnabled(modelName,input.gadgets,input.options);
    if(!loop.dispatcher().sameLoop(pending.dispatcher()))
      throw std::invalid_argument("Tool-search await crossed event loops");
    auto self=shared_from_this();
    pending.then([self](const bool* enabled,const std::exception_ptr& error){
      if(error){self->fail(error);return;}
      try{
        self->checkStop();
        if(!enabled)throw std::logic_error("Tool-search resolved without value");
        self->afterGadgetSearch(*enabled);
      }catch(...){self->fail(std::current_exception());}
    });
  }
  void afterGadgetSearch(bool enabled){
    checkStop();
    if(awaits.afterGadgetSearchSegment){
      result.resolve(awaits.afterGadgetSearchSegment(input,services,
        std::move(seed),enabled));
      return;
    }
    seed=model::prepareQueryModelUntilStashedImports(input,services,
      std::move(seed),enabled);
    if(!seed.stashedFeature){afterStashedImports();return;}
    if(!awaits.loadStashedModule||!awaits.loadBetasModule)
      throw std::invalid_argument("Awaited cached module imports are required");
    auto first=awaits.loadStashedModule();
    if(!loop.dispatcher().sameLoop(first.dispatcher()))
      throw std::invalid_argument("Cached module import crossed event loops");
    auto self=shared_from_this();
    first.then([self](const event_loop::Unit*,const std::exception_ptr& error){
      if(error){self->fail(error);return;}
      try{
        self->checkStop();
        auto second=self->awaits.loadBetasModule();
        if(!self->loop.dispatcher().sameLoop(second.dispatcher()))
          throw std::invalid_argument("Betas import crossed event loops");
        second.then([self](const event_loop::Unit*,
            const std::exception_ptr& nextError){
          if(nextError){self->fail(nextError);return;}
          try{self->afterStashedImports();}
          catch(...){self->fail(std::current_exception());}
        });
      }catch(...){self->fail(std::current_exception());}
    });
  }
  void afterStashedImports(){
    checkStop();
    if(awaits.afterStashedImportsSegment){
      result.resolve(awaits.afterStashedImportsSegment(input,services,
        std::move(seed)));
      return;
    }
    seed=model::prepareQueryModelUntilSchemas(input,services,std::move(seed));
    if(!awaits.gadgetToAPISchema)
      throw std::invalid_argument("Awaited tool-schema import is required");
    std::vector<event_loop::Deferred<Json>> schemas;
    for(const Json& gadget:seed.filteredGadgets){
      const bool defer=model::shouldDeferGadgetForPreflight(seed,gadget,services);
      auto pending=awaits.gadgetToAPISchema(gadget,defer,input.gadgets,input.options);
      if(!loop.dispatcher().sameLoop(pending.dispatcher()))
        throw std::invalid_argument("Tool-schema await crossed event loops");
      schemas.push_back(std::move(pending));
    }
    auto all=ownerPromiseAll(loop,schemas);
    auto self=shared_from_this();
    all.then([self](const std::vector<Json>* values,
        const std::exception_ptr& error){
      if(error){self->fail(error);return;}
      try{
        self->checkStop();
        if(!values)throw std::logic_error("Schema Promise.all lacked values");
        Json ordered=Json::array();
        for(const Json& schema:*values)ordered.push_back(schema);
        auto prepared=self->awaits.afterSchemasSegment?
          self->awaits.afterSchemasSegment(self->input,self->services,
            std::move(self->seed),std::move(ordered)):
          model::prepareQueryModelAfterSchemas(self->input,self->services,
            std::move(self->seed),std::move(ordered));
        self->result.resolve(std::move(prepared));
      }catch(...){self->fail(std::current_exception());}
    });
  }
};
} // namespace
event_loop::Deferred<model::PreflightState> startOwnerPreflight(
    event_loop::Loop& loop,model::PreflightInputs input,
    model::PreflightServices services,OwnerPreflightAwaits awaits,
    std::stop_token stop){
  return startOwnerPreflight(loop,std::make_shared<const model::PreflightInputs>(
    std::move(input)),std::move(services),std::move(awaits),stop);
}
event_loop::Deferred<model::PreflightState> startOwnerPreflight(
    event_loop::Loop& loop,std::shared_ptr<const model::PreflightInputs> input,
    model::PreflightServices services,OwnerPreflightAwaits awaits,
    std::stop_token stop){
  if(!loop.isOwnerThread())throw std::logic_error("Owner preflight requires loop owner");
  if(!input)throw std::invalid_argument("Owner preflight input is required");
  auto run=std::make_shared<Run>(loop,std::move(input),std::move(services),
    std::move(awaits),stop);
  auto result=run->result;
  run->begin();
  return result;
}
} // namespace batbox::query
