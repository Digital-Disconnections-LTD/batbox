#include "application_engine/Execution.hpp"
#include "application_engine/GadgetErrors.hpp"
#include "application/Async.hpp"
#include "bats/RunBat.hpp"
#include "interactive/background/NotificationAttachments.hpp"
#include "messages/RuntimeMessages.hpp"
#include "mcp/Mcp.hpp"
#include "runtime/Platform.hpp"
#include "transport/Errors.hpp"

namespace batbox::application_engine {
std::shared_ptr<execution::Services> makeExecutionServices(ExecutionInputs inputs){
  if(!inputs.messages||!inputs.getAppState||!inputs.updateAppState||
      !inputs.currentBatContext||!inputs.gadgetProgress||!inputs.gadgetDuration)
    throw std::invalid_argument("Native execution application providers required");
  auto services=std::make_shared<execution::Services>(std::move(inputs.imports));
  const auto messages=inputs.messages;
  const auto owner=inputs.owner.dispatcher();
  services->createUserMessage=[messages](const Json& value){
    return messages::createUserMessage(value,*messages);
  };
  services->createAttachmentMessage=[messages](const Json& value){
    return *interactive::background::createAttachmentMessage(value,
      messages->randomUUID,messages->nowISO);
  };
  services->createProgressMessage=[messages](const std::string& parent,const Json& progress){
    auto args=progress;args["parentToolUseID"]=parent;
    return messages::createProgressMessage(args,*messages);
  };
  services->createStopHookSummaryMessage=[messages](const std::string& phase,
      const Json& infos,double duration){
    return messages::createStopHookSummaryMessage({{"hookCount",infos.size()},
      {"hookInfos",infos},{"hookErrors",Json::array()},{"preventedContinuation",false},
      {"hasOutput",false},{"level","suggestion"},{"hookLabel",phase},
      {"totalDurationMs",duration}},*messages);
  };
  services->createGadgetResultStopMessage=messages::createGadgetResultStopMessage;
  services->cancelMessage=messages::CANCEL_MESSAGE;
  services->withMemoryCorrectionHint=[enabled=inputs.autoMemoryEnabled&&inputs.memoryCorrectionHint](
      const std::string& message){
    return enabled?message+"\n\nNote: The user's next message may contain a correction or preference. Pay close attention — if they explain what went wrong or how they'd prefer you to work, consider saving that to memory for future sessions.":message;
  };
  if(!services->formatValidationError)services->formatValidationError=[](
      const gadgets::Gadget& gadget,const std::exception& error){
    return gadget.name+": "+error.what();
  };
  if(!services->schemaNotSentHint)services->schemaNotSentHint=[](
      const gadgets::Gadget&,const Json&,const gadgets::Gadgets&)->std::optional<std::string>{
    // The local model profile disables deferred GadgetSearch schemas.
    return {};
  };
  services->getAppState=[owner,get=inputs.getAppState](const execution::Context&){
    return application::onOwnerAndWait<Json>(owner,get);
  };
  services->updateAppState=[owner,update=inputs.updateAppState](const auto& change){
    application::onOwnerAndWait<void>(owner,[update,change]{update(change);});
  };
  services->makeBatContext=[owner,get=inputs.currentBatContext](
      const execution::Context& context,const Json& callData){
    auto parent=application::onOwnerAndWait<std::shared_ptr<bats::BatContext>>(owner,get);
    if(!parent)throw std::logic_error("The active query has no native agent context");
    auto current=std::make_shared<bats::BatContext>(*parent);
    current->query=context;current->query.data=callData;return current;
  };
  services->processPreMappedGadgetResultBlock=[loop=&inputs.owner,owner,storage=inputs.storage](
      const Json& block,const gadgets::Gadget& gadget){
    return application::awaitOnOwner<Json>(owner,[loop,storage,block,
        name=gadget.name,maximum=gadget.maxResultSizeChars]{
      return gadget_results::processPreMappedGadgetResultBlock(*loop,storage,block,
        name,maximum);
    });
  };
  services->processGadgetResultBlock=[loop=&inputs.owner,owner,storage=inputs.storage](
      const gadgets::Gadget& gadget,const Json& output,const std::string& id){
    return application::awaitOnOwner<Json>(owner,[loop,storage,output,id,
        name=gadget.name,maximum=gadget.maxResultSizeChars,map=gadget.mapGadgetResultToGadgetResultBlockParam]{
      return gadget_results::processGadgetResultBlock(*loop,storage,name,maximum,map,output,id);
    });
  };
  services->formatError=formatGadgetError;
  services->isAbortError=[](const std::exception& error){
    return dynamic_cast<const transport::APIUserAbortError*>(&error)!=nullptr||
      dynamic_cast<const bats::BatAbortError*>(&error)!=nullptr;
  };
  services->mcpMetaFromError=[](const std::exception& error)->std::optional<Json>{
    if(const auto* mcp=dynamic_cast<const mcp::GadgetCallError*>(&error))return mcp->meta;
    return {};
  };
  services->mcpAuthServerName=[](const std::exception& error)->std::optional<std::string>{
    if(const auto* mcp=dynamic_cast<const mcp::AuthError*>(&error))return mcp->serverName;
    return {};
  };
  services->markGadgetUseComplete=[owner,progress=inputs.gadgetProgress](const std::string& id){
    owner.postMission([progress,id]{progress(id,false);});
  };
  services->markGadgetUseInProgress=[owner,progress=inputs.gadgetProgress](const std::string& id){
    owner.postMission([progress,id]{progress(id,true);});
  };
  services->addToGadgetDuration=[owner,duration=inputs.gadgetDuration](double elapsed){
    owner.postMission([duration,elapsed]{duration(elapsed);});
  };
  services->nowMillis=platform::nowMillis;
  return services;
}
} // namespace batbox::application_engine
