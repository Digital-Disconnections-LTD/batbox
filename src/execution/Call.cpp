#include "execution/Internal.hpp"
#include <algorithm>

namespace batbox::execution::internal {
namespace {
Json userArgs(const Json& content,const Json& assistant,const Json& gadgetUseResult,
              const std::optional<Json>& imageIds=std::nullopt,const std::optional<Json>& mcpMeta=std::nullopt) {
  Json args={{"content",content},{"sourceToolAssistantUUID",field(assistant,"uuid")}};
  if(!gadgetUseResult.is_discarded())args["toolUseResult"]=gadgetUseResult;
  if(imageIds)args["imagePasteIds"]=*imageIds;
  if(mcpMeta)args["mcpMeta"]=*mcpMeta;
  return args;
}
Json imageIdsFor(const Json& blocks,const Json& messages) {
  int count=0;
  for(const Json& block:blocks)if(field(block,"type")=="image")++count;
  if(count==0)return Json();
  int maxId=0;
  if(messages.is_array())for(const Json& message:messages) {
    if(field(message,"type")!="user")continue;
    const Json& ids=field(message,"imagePasteIds");
    if(ids.is_array())for(const Json& id:ids)if(id.is_number_integer())maxId=std::max(maxId,id.get<int>());
  }
  Json result=Json::array();
  for(int i=1;i<=count;++i)result.push_back(maxId+i);
  return result;
}
void appendHookInfo(Json& infos,const Json& message) {
  const Json& attachment=field(message,"attachment");
  if(attachment.is_object()&&!field(attachment,"command").is_null()&&
     !field(attachment,"durationMs").is_null())
    infos.push_back(Json{{"command",field(attachment,"command")},{"durationMs",field(attachment,"durationMs")}});
}
Json resultMessage(const Services& services,const Json& args) {
  require(services.createUserMessage,"createUserMessage");
  return services.createUserMessage(args);
}
void emitProgress(const Json& progress,const std::string& gadgetUseID,Services& services,
                  const std::function<void(const Update&)>& yield) {
  require(services.createProgressMessage,"createProgressMessage");
  yield(Update{services.createProgressMessage(gadgetUseID,progress),{},gadgetUseID});
}
void flush(const std::vector<Update>& buffered,const std::function<void(const Update&)>& yield) {
  for(const auto& update:buffered)yield(update);
}
}

void checkPermissionsAndCallGadget(const gadgets::Gadget& gadget,const Json& gadgetUse,
  const Json& assistantMessage,Context& context,Services& services,
  const std::function<void(const Update&)>& yield) {
  const std::string gadgetUseID=stringField(gadgetUse,"id");
  const Json& input=field(gadgetUse,"input");
  require(gadget.parseInput,"Tool.parseInput");
  Json parsedInput;
  try {parsedInput=gadget.parseInput(input);}
  catch(const std::exception& error) {
    require(services.formatValidationError,"formatValidationError");
    require(services.schemaNotSentHint,"schemaNotSentHint");
    std::string formatted=services.formatValidationError(gadget,error);
    const std::string raw=error.what();
    if(const auto hint=services.schemaNotSentHint(gadget,field(context.data,"messages"),context.gadgets))
      formatted+=*hint;
    Json content=Json::array({createResult(gadgetUseID,
      "<tool_use_error>InputValidationError: "+formatted+"</tool_use_error>")});
    yield(Update{resultMessage(services,userArgs(content,assistantMessage,
      "InputValidationError: "+raw)),{},gadgetUseID});
    return;
  }
  if(gadget.validateInputWithContext||gadget.validateInput) {
    gadgets::GadgetCallContext validationContext;
    validationContext.data=context.data;
    validationContext.stopToken=context.stop;
    validationContext.readFileState=context.readFileState;
    const auto validation=gadget.validateInputWithContext?
      gadget.validateInputWithContext(parsedInput,validationContext).get():
      gadget.validateInput(parsedInput,context.data).get();
    if(!validation.result) {
      const std::string message=validation.message.value_or("undefined");
      Json content=Json::array({createResult(gadgetUseID,"<tool_use_error>"+message+"</tool_use_error>")});
      yield(Update{resultMessage(services,userArgs(content,assistantMessage,
        "Error: "+message)),{},gadgetUseID});
      return;
    }
  }
  if(gadget.name==services.bashGadgetName && parsedInput.is_object() && parsedInput.contains("command")) {
    require(services.startSpeculativeClassifierCheck,"startSpeculativeClassifierCheck");
    require(services.getAppState,"getAppState");
    const Json appState=effectiveAppState(context,services.getAppState(context));
    services.startSpeculativeClassifierCheck(stringField(parsedInput,"command"),
      field(appState,"toolPermissionContext"),context);
  }
  Json processedInput=parsedInput;
  if(gadget.name==services.bashGadgetName && processedInput.is_object())processedInput.erase("_simulatedSedEdit");
  Json callInput=processedInput;
  Json backfilledClone;
  bool hasBackfill=false,processedIsBackfill=false;
  if(gadget.backfillObservableInput && processedInput.is_object()) {
    backfilledClone=processedInput;
    gadget.backfillObservableInput(backfilledClone);
    processedInput=backfilledClone;
    hasBackfill=processedIsBackfill=true;
  }
  Json hookPermissionResult;
  Json preHookInfos=Json::array();
  std::vector<Update> buffered;
  bool shouldPreventContinuation=false;
  std::string stopReason;
  require(services.nowMillis,"nowMillis");
  const auto preStart=services.nowMillis();
  require(services.preGadgetUseHooks,"preToolUseHooks");
  bool stopped=false;
  services.preGadgetUseHooks(gadget,processedInput,gadgetUse,context,[&](const Json& event){
    const auto type=stringField(event,"type");
    if(type=="message") {
      const Json& update=field(event,"message");
      const Json& message=field(update,"message");
      if(field(field(message,"message"),"type")=="progress" || field(message,"type")=="progress")
        emitProgress(message,gadgetUseID,services,yield);
      else {buffered.push_back(Update{message,{},gadgetUseID});appendHookInfo(preHookInfos,message);}
    } else if(type=="hookPermissionResult")hookPermissionResult=field(event,"hookPermissionResult");
    else if(type=="hookUpdatedInput") {processedInput=field(event,"updatedInput");
      processedIsBackfill=value::truthy(field(event,"inputIsBackfilledClone"));}
    else if(type=="preventContinuation")shouldPreventContinuation=value::truthy(field(event,"shouldPreventContinuation"));
    else if(type=="stopReason")stopReason=stringField(event,"stopReason");
    else if(type=="additionalContext") {
      const Json& message=field(field(event,"message"),"message");
      buffered.push_back(Update{message,{},gadgetUseID});
    } else if(type=="stop") {
      require(services.createGadgetResultStopMessage,"createToolResultStopMessage");
      Json content=Json::array({services.createGadgetResultStopMessage(gadgetUseID)});
      buffered.push_back(Update{resultMessage(services,userArgs(content,assistantMessage,
        "Error: "+stopReason)),{},gadgetUseID});
      stopped=true;
    }
  });
  if(stopped){flush(buffered,yield);return;}
  const double preHookDuration=services.nowMillis()-preStart;
  if(services.internalUser && !preHookInfos.empty() && preHookDuration>500) {
    require(services.createStopHookSummaryMessage,"createStopHookSummaryMessage");
    buffered.push_back(Update{services.createStopHookSummaryMessage("PreToolUse",preHookInfos,preHookDuration),{},gadgetUseID});
  }
  require(services.resolvePermissionDecision,"resolvePermissionDecision");
  const Json resolved=services.resolvePermissionDecision(hookPermissionResult,gadget,processedInput,
    context,assistantMessage,gadgetUseID);
  const Json& decision=field(resolved,"decision");
  const auto behavior=stringField(decision,"behavior");
  const Json& resolvedInput=field(resolved,"input");
  if(resolved.is_object()&&resolved.contains("input")){
    const bool changed=resolvedInput!=processedInput;
    processedInput=resolvedInput;
    processedIsBackfill=resolved.contains("inputIsBackfilledClone")?
      value::truthy(field(resolved,"inputIsBackfilledClone")):(processedIsBackfill&&!changed);
  }
  const Json& reason=field(decision,"decisionReason");
  if(field(reason,"type")=="hook" && field(reason,"hookName")=="PermissionRequest" && behavior!="ask") {
    require(services.createAttachmentMessage,"createAttachmentMessage");
    buffered.push_back(Update{services.createAttachmentMessage(Json{{"type","hook_permission_decision"},
      {"decision",behavior},{"toolUseID",gadgetUseID},{"hookEvent","PermissionRequest"}}),{},gadgetUseID});
  }
  if(behavior!="allow") {
    Json errorMessage=field(decision,"message");
    if(shouldPreventContinuation && !value::truthy(errorMessage))
      errorMessage="Execution stopped by PreToolUse hook"+(stopReason.empty()?std::string{}:std::string(": ")+stopReason);
    Json content=Json::array({createResult(gadgetUseID,errorMessage)});
    const Json& rejectBlocks=behavior=="ask"?field(decision,"contentBlocks"):Json();
    if(rejectBlocks.is_array())for(const Json& block:rejectBlocks)content.push_back(block);
    const Json imageIds=rejectBlocks.is_array()?imageIdsFor(rejectBlocks,field(context.data,"messages")):Json();
    buffered.push_back(Update{resultMessage(services,userArgs(content,assistantMessage,
      "Error: "+(errorMessage.is_string()?errorMessage.get<std::string>():std::string("undefined")),
      imageIds.is_null()?std::nullopt:std::optional<Json>(imageIds))),{},gadgetUseID});
    if(services.transcriptClassifierFeature && field(reason,"type")=="classifier" &&
       field(reason,"classifier")=="auto-mode") {
      require(services.permissionDeniedHooks,"permissionDeniedHooks");
      bool retry=false;
      services.permissionDeniedHooks(gadget,gadgetUseID,processedInput,context,[&](const Json& event){
        if(value::truthy(field(event,"retry")))retry=true;
      });
      if(retry)buffered.push_back(Update{resultMessage(services,Json{{"content",
        "The PermissionDenied hook indicated this command is now approved. You may retry it if you would like."},
        {"isMeta",true}}),{},gadgetUseID});
    }
    flush(buffered,yield);
    return;
  }
  if(decision.is_object()&&decision.contains("updatedInput")){
    processedInput=field(decision,"updatedInput");
    processedIsBackfill=value::truthy(field(decision,"updatedInputIsBackfilledClone"));
  }
  if(hasBackfill && processedInput.is_object() && callInput.is_object() &&
     processedInput.contains("file_path") && callInput.contains("file_path") &&
     processedInput["file_path"]==backfilledClone["file_path"]) {
    callInput=processedInput;
    callInput["file_path"]=parsedInput["file_path"];
  } else if(!processedIsBackfill)callInput=processedInput;

  require(gadget.call,"Tool.call");
  require(services.canUseGadget,"canUseTool");
  require(services.addToGadgetDuration,"addToToolDuration");
  const auto startTime=services.nowMillis();
  try {
    Json callContext=context.data;
    callContext["toolUseId"]=gadgetUseID;
    callContext["userModified"]=value::truthy(field(decision,"userModified"));
    const auto liveReason=services.currentAbortReason;
    const std::string initialReason=liveReason?liveReason():context.abortReason;
    if(!initialReason.empty())callContext["abortReason"]=initialReason;
    gadgets::GadgetCallContext indigenousContext{callContext,context.stop,
      [liveReason,fallback=context.abortReason]{return liveReason?liveReason():fallback;}};
    indigenousContext.readFileState=context.readFileState;
    indigenousContext.nestedMemoryAttachmentTriggers=
      context.nestedMemoryAttachmentTriggers;
    indigenousContext.loadedNestedMemoryPaths=context.loadedNestedMemoryPaths;
    indigenousContext.dynamicSkillDirTriggers=context.dynamicSkillDirTriggers;
    indigenousContext.discoveredSkillNames=context.discoveredSkillNames;
    if(services.makeBatContext)
      indigenousContext.batContext=services.makeBatContext(context,callContext);
    const auto result=gadget.call(callInput,indigenousContext,services.canUseGadget,assistantMessage,
      [&](const gadgets::GadgetProgress& progress){
        emitProgress(Json{{"toolUseID",progress.gadgetUseID},{"data",progress.data}},gadgetUseID,services,yield);
      }).get();
    services.addToGadgetDuration(services.nowMillis()-startTime);
    if(result.structuredOutput) {
      require(services.createAttachmentMessage,"createAttachmentMessage");
      buffered.push_back(Update{services.createAttachmentMessage(Json{{"type","structured_output"},
        {"data",*result.structuredOutput}}),{},gadgetUseID});
    }
    if(!gadget.mapGadgetResultToGadgetResultBlockParam && !gadget.mapGadgetResultWithPresentationMetadata)
      throw std::invalid_argument("Unbound tool execution import: Tool.mapToolResultToToolResultBlockParam");
    const Json mapped=gadget.mapGadgetResultWithPresentationMetadata?
      gadget.mapGadgetResultWithPresentationMetadata(result.data,gadgetUseID,result.presentationMetadata):
      gadget.mapGadgetResultToGadgetResultBlockParam(result.data,gadgetUseID);
    Json output=result.data;
    std::vector<Update> hookResults;
    const bool isMcp=gadget.isMcp.value_or(false);
    auto addGadgetResult=[&](const Json& gadgetOutput,const std::optional<Json>& preMapped){
      Json block;
      if(preMapped){require(services.processPreMappedGadgetResultBlock,"processPreMappedToolResultBlock");
        block=services.processPreMappedGadgetResultBlock(*preMapped,gadget);}
      else {require(services.processGadgetResultBlock,"processToolResultBlock");
        block=services.processGadgetResultBlock(gadget,gadgetOutput,gadgetUseID);}
      Json blocks=Json::array({block});
      if(value::truthy(field(decision,"acceptFeedback")))blocks.push_back(Json{{"type","text"},
        {"text",field(decision,"acceptFeedback")}});
      const Json& allowed=field(decision,"contentBlocks");
      if(allowed.is_array())for(const Json& b:allowed)blocks.push_back(b);
      const Json imageIds=allowed.is_array()?imageIdsFor(allowed,field(context.data,"messages")):Json();
      const bool bat=value::truthy(field(context.data,"agentId"));
      const bool preserve=value::truthy(field(context.data,"preserveToolUseResults"));
      Json keptResult=bat&&!preserve?Json(Json::value_t::discarded):Json(gadgetOutput);
      std::optional<Json> meta=!bat&&!result.mcpMeta.is_null()?std::optional<Json>(result.mcpMeta):std::nullopt;
      Update update{resultMessage(services,userArgs(blocks,assistantMessage,keptResult,
        imageIds.is_null()?std::nullopt:std::optional<Json>(imageIds),meta)),{},gadgetUseID};
      if(result.contextModifier)update.contextModifier=result.contextModifier;
      if(result.appStateModifier)update.appStateModifier=result.appStateModifier;
      buffered.push_back(std::move(update));
    };
    if(!isMcp)addGadgetResult(output,mapped);
    require(services.postGadgetUseHooks,"postToolUseHooks");
    Json postHookInfos=Json::array();
    const auto postStart=services.nowMillis();
    services.postGadgetUseHooks(gadget,processedInput,output,context,[&](const Json& hook){
      if(hook.contains("updatedMCPToolOutput")) {
        if(isMcp)output=hook["updatedMCPToolOutput"];
      } else {
        const Json& message=field(hook,"message");
        if(isMcp)hookResults.push_back(Update{message,{},gadgetUseID});
        else buffered.push_back(Update{message,{},gadgetUseID});
        appendHookInfo(postHookInfos,message);
      }
    });
    const double postDuration=services.nowMillis()-postStart;
    if(isMcp)addGadgetResult(output,std::nullopt);
    if(services.internalUser && !postHookInfos.empty() && postDuration>500) {
      require(services.createStopHookSummaryMessage,"createStopHookSummaryMessage");
      buffered.push_back(Update{services.createStopHookSummaryMessage("PostToolUse",postHookInfos,postDuration),{},gadgetUseID});
    }
    if(result.newMessages)for(const Json& message:*result.newMessages)
      buffered.push_back(Update{message,{},gadgetUseID});
    if(shouldPreventContinuation) {
      require(services.createAttachmentMessage,"createAttachmentMessage");
      buffered.push_back(Update{services.createAttachmentMessage(Json{{"type","hook_stopped_continuation"},
        {"message",stopReason.empty()?"Execution stopped by hook":stopReason},
        {"hookName","PreToolUse:"+gadget.name},{"toolUseID",gadgetUseID},{"hookEvent","PreToolUse"}}),{},gadgetUseID});
    }
    buffered.insert(buffered.end(),hookResults.begin(),hookResults.end());
    flush(buffered,yield);
  } catch(const std::exception& error) {
    services.addToGadgetDuration(services.nowMillis()-startTime);
    require(services.mcpAuthServerName,"mcpAuthServerName");
    if(const auto serverName=services.mcpAuthServerName(error)){
      require(services.updateAppState,"updateAppState");
      services.updateAppState([serverName](const Json& previous){
        const Json& mcp=field(previous,"mcp");
        const Json& clients=field(mcp,"clients");
        if(!clients.is_array())return previous;
        for(std::size_t i=0;i<clients.size();++i){
          const Json& client=clients[i];
          if(field(client,"name")!=*serverName)continue;
          if(field(client,"type")!="connected")return previous;
          Json next=previous;
          Json changed={{"name",*serverName},{"type","needs-auth"}};
          if(client.is_object()&&client.contains("config"))changed["config"]=client["config"];
          next["mcp"]["clients"][i]=std::move(changed);
          return next;
        }
        return previous;
      });
    }
    require(services.formatError,"formatError");
    const std::string content=services.formatError(error);
    require(services.isAbortError,"isAbortError");
    require(services.postGadgetUseFailureHooks,"postToolUseFailureHooks");
    std::vector<Update> failureHooks;
    services.postGadgetUseFailureHooks(gadget,processedInput,content,services.isAbortError(error),context,[&](const Json& message){
      failureHooks.push_back(Update{field(message,"message"),{},gadgetUseID});
    });
    std::optional<Json> meta;
    if(!value::truthy(field(context.data,"agentId"))) {
      require(services.mcpMetaFromError,"mcpMetaFromError");
      meta=services.mcpMetaFromError(error);
    }
    Json blocks=Json::array({createResult(gadgetUseID,content)});
    buffered.push_back(Update{resultMessage(services,userArgs(blocks,assistantMessage,
      "Error: "+content,std::nullopt,meta)),{},gadgetUseID});
    buffered.insert(buffered.end(),failureHooks.begin(),failureHooks.end());
    flush(buffered,yield);
  }
}

} // namespace batbox::execution::internal
