#include "application_engine/Compactor.hpp"
#include "application_engine/AutoPolicy.hpp"
#include "application/Async.hpp"
#include "bats/ForkedBat.hpp"
#include "utils/tokens.hpp"

namespace batbox::application_engine {
namespace {
struct CursorBridge {
  event_loop::Loop::Dispatcher owner;
  query::AsyncModelCursor cursor;
  ~CursorBridge(){
    try{
      if(cursor.close&&owner.valid())owner.postMission([close=cursor.close]{close();});
    }catch(...){}
  }
};
}
compaction::SummaryServices Compactor::summaryFor(const execution::Context& context){
  auto services=inputs_.summary;
  const auto feature=inputs_.messages->featureGate;
  services.feature=feature;
  services.boundary.randomUUID=inputs_.messages->randomUUID;
  services.boundary.nowISO=inputs_.messages->nowISO;
  services.boundary.feature=feature;
  services.normalizeMessagesForAPI=[normalize=inputs_.normalize,gadgets=context.gadgets](
      const Json& messages,const Json&){return normalize(messages,gadgets);};
  services.getMaxOutputTokensForModel=[model=inputs_.model](const std::string& name){
    const auto profile=model->findProfile(name);
    if(!profile)throw std::out_of_range("Model profile unavailable: "+name);
    return profile->maxOutputTokens;
  };
  const auto* read=gadgets::findGadgetByName(context.gadgets,"Read");
  if(!read)throw std::invalid_argument("Compaction requires the Read tool schema");
  services.fileReadGadget={{"name",read->name}};
  services.queryModelWithStreaming=[model=inputs_.model,
      owner=inputs_.owner.dispatcher(),gadgets=context.gadgets](
      const compaction::SummaryModelRequest& input){
    query::ModelRequest request;
    request.messages=input.messages;request.systemPoetry=input.systemPoetry;
    request.thinkingConfig=input.thinkingConfig;request.signal=input.signal;
    request.options=input.options;request.getGadgetPermissionContext=input.getGadgetPermissionContext;
    for(const auto& descriptor:input.gadgets){
      const auto name=descriptor.at("name").get<std::string>();
      const auto* gadget=gadgets::findGadgetByName(gadgets,name);
      if(!gadget)throw std::invalid_argument("Compaction tool is unavailable: "+name);
      request.gadgets.push_back(gadget);
    }
    auto bridge=std::make_shared<CursorBridge>();bridge->owner=owner;
    bridge->cursor=application::onOwnerAndWait<query::AsyncModelCursor>(owner,
      [model,request]{return model->makeCursor(request);});
    return model::NextMessage([bridge]{
      return application::awaitOnOwner<std::optional<model::MessageRef>>(
        bridge->owner,[bridge]{return bridge->cursor.next();});
    });
  };
  return services;
}
compaction::AutoServices Compactor::automaticServices(const execution::Context& query,
    std::shared_ptr<bats::StashSafeParams> indigenous){
  compaction::AutoServices services;
  const bool settingEnabled=inputs_.autoCompactEnabled?
    inputs_.autoCompactEnabled():true;
  const Json settings=inputs_.compactionSettings?inputs_.compactionSettings():Json::object();
  const auto policy=[config=inputs_.config,model=inputs_.model,settingEnabled,settings](
      const std::string& name){
    return autoCompactDependencies(config,settingEnabled,name,model.get(),settings);
  };
  const auto local=policy(inputs_.config.model);
  services.environment=local.env;
  services.feature=inputs_.messages->featureGate;
  services.isAutoCompactEnabled=[local]{return batbox::isAutoCompactEnabled(local);};
  services.reactiveOnly=[]{return false;};
  services.isContextCollapseEnabled=[]{return false;};
  const auto normalize=inputs_.normalizeAttachment;
  services.tokenCountWithEstimation=[normalize](const Json& messages){
    return tokenCountWithEstimation(messages,normalize);
  };
  services.getAutoCompactThreshold=[policy](const std::string& name){
    return batbox::getAutoCompactThreshold(policy(name));
  };
  services.calculateTokenWarningState=[policy](double tokens,const std::string& name){
    return batbox::calculateTokenWarningState(tokens,policy(name));
  };
  // Flight-memory compaction is a distinct optional path. The ordinary
  // summary workflow remains available when that feature has no stored summary.
  services.tryFlightMemoryCompaction=[](const Json&,const Json&,double){return Json();};
  services.clearLastSummarizedMessageId=[]{ };
  services.runPostCompactCleanup=[owner=inputs_.owner.dispatcher(),after=inputs_.cleanupAfterCompact](const auto& source){
    application::onOwnerAndWait<void>(owner,[after,source]{after(source);});
  };
  services.markPostCompaction=inputs_.markPostCompaction;
  auto self=shared_from_this();
  services.compactConversation=[self,query,indigenous=std::move(indigenous)](
      const Json& messages,const Json&,const Json& stash,bool suppress,
      const std::optional<std::string>& instructions,bool automatic,const Json& tracking){
    auto context=self->contextFor(query,indigenous);
    auto workflow=self->workflowFor(query);
    return compaction::compactConversation(messages,context,stash,suppress,
      instructions,automatic,tracking,workflow);
  };
  return services;
}
event_loop::Deferred<Json> Compactor::automatic(const Json& messages,
    const execution::Context& context,const Json& stash,const std::string& source,
    const Json& tracking,double freed,std::shared_ptr<bats::StashSafeParams> indigenous){
  auto services=automaticServices(context,std::move(indigenous));
  const auto skipped=[this]{
    event_loop::Deferred<Json> result(inputs_.owner);
    result.resolve(Json{{"wasCompacted",false}});return result;
  };
  const auto disabled=services.environment.find("BATBOX_DISABLE_COMPACT");
  if(disabled!=services.environment.end()&&isEnvTruthy(Json(disabled->second)))
    return skipped();
  const auto& failures=value::property(tracking,"consecutiveFailures");
  if(failures.is_number()&&failures.get<double>()>=3)return skipped();
  const auto& model=value::property(value::property(context.data,"options"),"mainLoopModel");
  if(!model.is_string())throw std::invalid_argument("Compaction model required");
  if(!compaction::shouldAutoCompact(messages,model.get<std::string>(),source,freed,services)){
    return skipped();
  }
  return application::runInWorker<Json>(inputs_.owner,
    [messages,context,stash,source,tracking,freed,services=std::move(services)]()mutable{
      return compaction::autoCompactIfNeeded(messages,context.data,stash,source,
        tracking,freed,services);
    });
}
} // namespace batbox::application_engine
