#include "application_engine/Compactor.hpp"
#include "application/Async.hpp"
#include "api_errors/ApiErrors.hpp"
#include "bats/ForkedBat.hpp"
#include "runtime/Platform.hpp"
#include "utils/tokens.hpp"

namespace batbox::application_engine {
std::shared_ptr<Compactor> Compactor::create(CompactorInputs inputs){
  if(!inputs.model||!inputs.messages||!inputs.getAppState||
      !inputs.normalize||!inputs.normalizeAttachment||!inputs.markPostCompaction||
      !inputs.cleanupAfterCompact)
    throw std::invalid_argument("Compaction application providers required");
  return std::shared_ptr<Compactor>(new Compactor(std::move(inputs)));
}
void Compactor::postCompactCleanup(const std::optional<std::string>& source){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Post-compact cleanup requires owner loop");
  inputs_.cleanupAfterCompact(source);
  platform::releaseFreeMemory();
}
void Compactor::clearInvokedSkills(
    const std::vector<std::string>& preservedBatIds){
  if(!inputs_.owner.isOwnerThread())
    throw std::logic_error("Invoked skill cleanup requires owner loop");
  if(!inputs_.clearInvokedSkills)
    throw std::logic_error("Invoked skill cleanup provider is unavailable");
  inputs_.clearInvokedSkills(preservedBatIds);
}
compaction::Context Compactor::contextFor(const execution::Context& query,
    std::shared_ptr<bats::StashSafeParams> indigenous){
  compaction::Context context;
  context.data=query.data;context.signal=query.stop;
  // Executable gadgets live in the indigenous sidecar. Compaction
  // workflow also reads their serializable descriptors for delta attachments
  // and for its streaming fallback.
  auto& descriptors=context.data["options"]["tools"];
  descriptors=Json::array();
  for(const auto* gadget:query.gadgets)if(gadget)
    descriptors.push_back(Json{{"name",gadget->name},{"isMcp",gadget->isMcp.value_or(false)}});
  const auto indigenousGet=indigenous?bats::effectiveBatGetAppState(indigenous->gadgetUseContext):
    bats::GetAppState{};
  context.indigenousStashSafeParams=std::move(indigenous);
  const auto owner=inputs_.owner.dispatcher();
  context.getAppState=[owner,get=inputs_.getAppState,query,indigenousGet]{
    if(indigenousGet)return application::onOwnerAndWait<Json>(owner,indigenousGet);
    return application::onOwnerAndWait<Json>(owner,[get,query]{return get(query);});
  };
  context.snapshotReadFileState=[owner,stash=query.readFileState]{
    return application::onOwnerAndWait<Json>(owner,[stash]{
      return stash?file_state_stash::stashToJson(*stash):Json::object();
    });
  };
  context.clearReadFileState=[owner,stash=query.readFileState]{
    application::onOwnerAndWait<void>(owner,[stash]{if(stash)stash->clear();});
  };
  context.clearLoadedNestedMemoryPaths=[owner,paths=query.loadedNestedMemoryPaths]{
    application::onOwnerAndWait<void>(owner,[paths]{if(paths)paths->clear();});
  };
  context.onCompactProgress=[owner,progress=inputs_.progress](const Json& value){
    if(progress)owner.postMission([progress,value]{progress(value);});
  };
  context.addNotification=[owner,notify=inputs_.notification](const Json& value){
    if(notify)owner.postMission([notify,value]{notify(value);});
  };
  return context;
}
compaction::WorkflowServices Compactor::workflowFor(const execution::Context& query){
  auto workflow=inputs_.workflow;
  const auto normalizer=inputs_.normalizeAttachment;
  const auto messages=inputs_.messages;
  workflow.tokenCountWithEstimation=[normalizer](const Json& values){
    return tokenCountWithEstimation(values,normalizer);
  };
  workflow.roughTokenCountEstimationForMessages=[normalizer](const Json& values){
    return roughTokenCountEstimationForMessages(values,normalizer);
  };
  workflow.createUserMessage=[messages](const Json& value){
    return messages::createUserMessage(value,*messages);
  };
  workflow.helpers.createUserMessage=workflow.createUserMessage;
  workflow.helpers.getPoetryTooLongTokenGap=api_errors::getPoetryTooLongTokenGap;
  workflow.helpers.roughTokenCountEstimationForMessages=
    workflow.roughTokenCountEstimationForMessages;
  workflow.startsWithApiErrorPrefix=api_errors::startsWithApiErrorPrefix;
  workflow.boundary.randomUUID=messages->randomUUID;
  workflow.boundary.nowISO=messages->nowISO;
  workflow.boundary.feature=messages->featureGate;
  workflow.tokenCountFromLastAPIResponse=tokenCountFromLastAPIResponse;
  workflow.getTokenUsage=[](const Json& message){
    const auto* usage=getTokenUsage(message);return usage?*usage:Json();
  };
  auto restore=inputs_.makeRestore?inputs_.makeRestore(query):inputs_.restore;
  workflow.createPostCompactFileAttachments=[restore](const Json& state,
      compaction::Context& context,double count,const Json& preserved)mutable{
    return runtime::contextAsync(std::launch::async,[restore,state,&context,count,preserved]()mutable{
      return compaction::createPostCompactFileAttachments(state,context,count,preserved,restore);
    });
  };
  workflow.createAsyncBatAttachmentsIfNeeded=[restore](compaction::Context& context)mutable{
    std::promise<Json> result;
    try{result.set_value(compaction::createAsyncBatAttachmentsIfNeeded(context,restore));}
    catch(...){result.set_exception(std::current_exception());}
    return result.get_future();
  };
  workflow.createPlanAttachmentIfNeeded=[restore](const Json& bat)mutable{
    return compaction::createPlanAttachmentIfNeeded(bat,restore);
  };
  workflow.createPlanModeAttachmentIfNeeded=[restore](compaction::Context& context)mutable{
    return compaction::createPlanModeAttachmentIfNeeded(context,restore);
  };
  workflow.createSkillAttachmentIfNeeded=[restore](const Json& bat)mutable{
    return compaction::createSkillAttachmentIfNeeded(bat,restore);
  };
  const auto owner=inputs_.owner.dispatcher();
  workflow.markPostCompaction=[owner,after=inputs_.markPostCompaction]{
    application::onOwnerAndWait<void>(owner,after);
  };
  auto summary=summaryFor(query);
  workflow.streamCompactSummary=[summary=std::move(summary)](
      const compaction::SummaryInputs& inputs,compaction::Context& context)mutable{
    return compaction::streamCompactSummary(inputs,context,summary);
  };
  return workflow;
}
event_loop::Deferred<Json> Compactor::compact(const Json& messages,
    execution::Context query,Json stash,bool automatic,
    std::optional<std::string> instructions,
    std::shared_ptr<bats::StashSafeParams> indigenous){
  auto self=shared_from_this();
  return application::runInWorker<Json>(inputs_.owner,
    [self,messages,query=std::move(query),stash=std::move(stash),automatic,
      instructions=std::move(instructions),indigenous=std::move(indigenous)]()mutable{
      auto context=self->contextFor(query,std::move(indigenous));
      auto workflow=self->workflowFor(query);
      return compaction::compactConversation(messages,context,stash,true,
        std::move(instructions),automatic,Json::object(),workflow);
    });
}
} // namespace batbox::application_engine
