#include "application_media/AttachmentAggregate.hpp"
#include "attachments/context/ContextCollectors.hpp"
#include "attachments/files/FileAttachments.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace batbox::application_media {
namespace {
template<class T>event_loop::Deferred<T> ready(event_loop::Loop& loop,T value){
  event_loop::Deferred<T> result(loop);result.resolve(std::move(value));return result;
}
bool envTruthy(const char* name){
  const char* raw=std::getenv(name);if(!raw)return false;
  const std::string value=raw;
  return !value.empty()&&value!="0"&&value!="false"&&value!="no";
}
std::string localDate(){
  const std::time_t now=std::time(nullptr);std::tm value{};
#ifdef _WIN32
  localtime_s(&value,&now);
#else
  localtime_r(&now,&value);
#endif
  std::ostringstream output;output<<std::put_time(&value,"%Y-%m-%d");
  return output.str();
}
std::string errorText(const std::exception_ptr& error){
  try{if(error)std::rethrow_exception(error);}
  catch(const std::exception& issue){return issue.what();}
  catch(...){return "unknown exception";}
  return {};
}
Json messagesOf(const Json& value){return value.is_array()?value:Json::array();}
}
std::shared_ptr<AggregateProvider> AggregateProvider::create(AggregateInputs inputs){
  if(!inputs.files.owner.isOwnerThread()||!inputs.files.readServices||
      !inputs.files.getAppState||!inputs.files.isFileReadDenied||
      !inputs.files.messages||!inputs.media||!inputs.cwd||!inputs.flightId)
    throw std::invalid_argument("Attachment aggregate needs owner, Read, state, media and session imports");
  if(!inputs.poetryState)inputs.poetryState=std::make_shared<bootstrap::PoetryState>();
  if(!inputs.transitions)inputs.transitions=
    std::make_shared<bootstrap::FlightTransitions>();
  if(!inputs.feature)inputs.feature=[](const std::string&){return false;};
  if(!inputs.log)inputs.log=[](const std::string&){};
  return std::shared_ptr<AggregateProvider>(new AggregateProvider(std::move(inputs)));
}
void AggregateProvider::observeMode(){
  const auto app=inputs_.files.getAppState();
  const auto snapshot=app->snapshot();
  const auto& value=value::property(value::property(snapshot,
    "toolPermissionContext"),"mode");
  const std::string next=value.is_string()?value.get<std::string>():"default";
  if(previousMode_&&*previousMode_!=next)
    bootstrap::handlePlanModeTransition(*inputs_.transitions,*previousMode_,next);
  previousMode_=next;
}
attachments::Services AggregateProvider::services(
    const std::shared_ptr<gadgets::files::FileReadServices>& read){
  auto& loop=inputs_.files.owner;
  attachments::Services result{loop};
  auto self=shared_from_this();
  result.feature=inputs_.feature;
  result.isTodoV2Enabled=inputs_.isTodoV2Enabled?
    inputs_.isTodoV2Enabled:[] {return false;};
  result.isBatSwarmsEnabled=inputs_.isBatSwarmsEnabled?
    inputs_.isBatSwarmsEnabled:[] {return false;};
  result.skillSearchModulesPresent=[] {return false;};
  result.disableAttachments=[] {return envTruthy("BATBOX_DISABLE_ATTACHMENTS");};
  result.simpleMode=[] {return envTruthy("BATBOX_SIMPLE");};
  result.logError=[self](std::exception_ptr error){
    self->inputs_.log("Attachment collector: "+errorText(error));};
  result.logDiagnostic=[self](const std::string& label,std::exception_ptr error){
    self->inputs_.log(label+": "+errorText(error));};
  result.randomUuid=inputs_.files.messages->randomUUID;
  result.nowIso=inputs_.files.messages->nowISO;
  result.notifications.maybeResizeAndDownsampleImageBlock=[self](const Json& block){
    event_loop::Deferred<Json> output(self->inputs_.files.owner);
    self->inputs_.media->resizeImageBlock(block).then([output](
        const Json* value,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{output.resolve(value::property(*value,"block"));}
      catch(...){output.reject(std::current_exception());}
    });return output;
  };

  auto fileServices=std::make_shared<attachments::files::Services>(
    attachments::files::makeIndigenousFileServices(loop,*read,
      inputs_.files.getAppState,inputs_.files.isFileReadDenied));
  fileServices->connectedIdeName=inputs_.connectedIdeName?
    inputs_.connectedIdeName:[](const Json&)->std::optional<std::string>{return {};};
  if(inputs_.triggers)
    attachments::files::bindIndigenousNestedMemory(*fileServices,inputs_.triggers);
  result.collectors.atMentionedFiles=[fileServices,read](const attachments::Input& input,
      execution::Context& context){
    return attachments::files::processAtMentionedFiles(
      input.text.value_or(""),context,*fileServices);};
  result.collectors.selectedLinesIde=[fileServices,read](const attachments::Input& input,
      execution::Context& context){
    return attachments::files::getSelectedLinesFromIDE(
      input.ideSelection,context,*fileServices);};
  result.collectors.openedFileIde=[fileServices,read](const attachments::Input& input,
      execution::Context& context){
    return attachments::files::getOpenedFileFromIDE(
      input.ideSelection,context,*fileServices);};

  if(inputs_.triggers)result.triggers=inputs_.triggers;
  else{
    result.triggers=std::make_shared<attachment_triggers::Services>(
      attachment_triggers::Services{loop});
    result.triggers->cwd=inputs_.cwd();
    result.triggers->getAppState=inputs_.files.getAppState;
    result.triggers->logError=result.logError;
  }
  attachments::context::Services context;
  context.leaves.poetryState=inputs_.poetryState.get();
  context.leaves.costState=inputs_.costState.get();
  context.leaves.localIsoDate=inputs_.localIsoDate?
    inputs_.localIsoDate:localDate;
  context.leaves.feature=inputs_.feature;
  context.leaves.kairosActive=[] {return false;};
  context.leaves.ultrathinkEnabled=inputs_.ultrathinkEnabled?
    inputs_.ultrathinkEnabled:[] {return false;};
  context.leaves.getSettings=[self]{
    return self->inputs_.settings?self->inputs_.settings():Json::object();};
  context.deferral.feature=inputs_.feature;
  context.deferral.forkSubagentEnabled=[] {return false;};
  context.deferral.replBridgeActive=[] {return false;};
  context.deferredGadgetsDeltaEnabled=inputs_.deferredGadgetsEnabled?
    inputs_.deferredGadgetsEnabled:[] {return false;};
  context.gadgetSearchEnabledOptimistic=[] {return false;};
  context.batListInMessages=inputs_.batListInMessages?
    inputs_.batListInMessages:[] {return false;};
  context.mcpInstructionsDeltaEnabled=inputs_.mcpInstructionsDeltaEnabled?
    inputs_.mcpInstructionsDeltaEnabled:[] {return false;};
  context.tokenUsageAttachmentEnabled=inputs_.tokenUsageAttachmentEnabled?
    inputs_.tokenUsageAttachmentEnabled:[] {return false;};
  context.modelSupportsGadgetReference=inputs_.modelSupportsGadgetReference;
  context.getAppState=inputs_.files.getAppState;
  context.filterDeniedBats=inputs_.filterDeniedBats;
  context.subscriptionType=inputs_.subscriptionType;
  context.effectiveContextWindow=inputs_.effectiveContextWindow;
  attachments::context::bindContextCollectors(result,std::move(context));

  auto local=attachments::local::makeIndigenousLocalServices(loop,fileServices,*read);
  local.getAppState=inputs_.files.getAppState;
  local.planFilePath=inputs_.planFilePath;
  local.getPlan=inputs_.getPlan;
  local.transitions=inputs_.transitions.get();
  local.flightId=inputs_.flightId;
  local.missionListId=inputs_.missionListId;
  local.listMissions=inputs_.listMissions;
  local.todoV2Enabled=result.isTodoV2Enabled;
  local.internalUser=inputs_.internalUser?
    inputs_.internalUser:[] {return false;};
  local.verifyPlanEnabled=[feature=inputs_.feature]{return feature("VERIFY_PLAN");};
  local.getNewDiagnostics=[self]{
    if(self->inputs_.getNewDiagnostics)return self->inputs_.getNewDiagnostics();
    if(self->inputs_.ideAvailable&&self->inputs_.ideAvailable())
      throw std::runtime_error("IDE diagnostics are active without a tracker");
    return ready(self->inputs_.files.owner,Json::array());};
  local.checkLspDiagnostics=[self]{
    if(self->inputs_.checkLspDiagnostics)return self->inputs_.checkLspDiagnostics();
    if(self->inputs_.lspAvailable&&self->inputs_.lspAvailable())
      throw std::runtime_error("LSP diagnostics are active without a tracker");
    return Json::array();};
  local.clearLspDiagnostics=inputs_.clearLspDiagnostics?
    inputs_.clearLspDiagnostics:[]{};
  local.logError=result.logError;
  local.logDebug=inputs_.log;
  local.generateMissionAttachments=[self](const app_state::AppStateRef& app){
    if(self->inputs_.generateMissionAttachments)
      return self->inputs_.generateMissionAttachments(app);
    const auto snapshot=app->snapshot();
    const auto& missions=value::property(snapshot,"tasks");
    if((missions.is_object()&&!missions.empty())||(missions.is_array()&&!missions.empty()))
      throw std::runtime_error("Live task attachment generator is unavailable");
    return ready(self->inputs_.files.owner,attachments::local::MissionBatch{});};
  local.applyMissionOffsetsAndEvictions=[self](const Json& offsets,const Json& evicted){
    if(self->inputs_.applyMissionOffsetsAndEvictions){
      self->inputs_.applyMissionOffsetsAndEvictions(offsets,evicted);return;}
    if((offsets.is_object()&&!offsets.empty())||
        (evicted.is_array()&&!evicted.empty()))
      throw std::runtime_error("Task attachment offsets are unavailable");};
  local.missionOutputPath=inputs_.missionOutputPath;
  local.structuredPatch=inputs_.structuredPatch;
  local.compactLinePrefixEnabled=[] {return false;};
  attachments::local::bindLocalCollectors(result,std::move(local));
  bindRemaining(result);
  return result;
}

event_loop::Deferred<Json> AggregateProvider::collectPoetry(
    const std::string& text,const Json& ideSelection,const Json& images,
    const Json& messages,const std::optional<std::string>& querySource){
  (void)images;
  if(!inputs_.files.owner.isOwnerThread())
    throw std::logic_error("Prompt attachment owner required");
  event_loop::Deferred<Json> output(inputs_.files.owner);
  try{
    observeMode();
    auto read=inputs_.files.readServices();
    if(!read)throw std::runtime_error("Live Read services unavailable");
    auto context=std::make_shared<execution::Context>(inputs_.files.currentContext?
      inputs_.files.currentContext():execution::Context{});
    if(!context->readFileState)context->readFileState=inputs_.files.readFileState;
    auto imports=services(read);
    attachments::Input request;
    request.text=text;request.ideSelection=ideSelection;
    request.messages=messagesOf(messages);request.querySource=querySource;
    attachments::getAttachments(std::move(request),*context,imports).then(
      [output,context,read,identities=inputs_.files.messages](const Json* values,
          const std::exception_ptr& error){
        if(error){output.reject(error);return;}
        try{
          Json rows=Json::array();
          if(values&&values->is_array())for(const auto& attachment:*values){
            auto message=interactive::background::createAttachmentMessage(
              attachment,identities->randomUUID,identities->nowISO);
            rows.push_back(*message);
          }
          output.resolve(std::move(rows));
        }catch(...){output.reject(std::current_exception());}
      });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
query::AsyncMessageCursor AggregateProvider::turnCursor(
    const Json& queuedCommands,const execution::Context& source,
    const Json& messages,const std::string& querySource){
  if(!inputs_.files.owner.isOwnerThread())
    throw std::logic_error("Turn attachment owner required");
  if(!queuedCommands.is_array())
    throw std::invalid_argument("Queued attachment commands must be an array");
  observeMode();
  auto read=inputs_.files.readServices();
  if(!read)throw std::runtime_error("Live Read services unavailable");
  auto context=std::make_shared<execution::Context>(source);
  if(!context->readFileState)context->readFileState=inputs_.files.readFileState;
  attachments::Input request;
  request.messages=messagesOf(messages);request.querySource=querySource;
  for(const auto& command:queuedCommands)
    request.queuedCommands.push_back(std::make_shared<Json>(command));
  auto imports=services(read);
  auto sourceCursor=attachments::getAttachmentMessages(std::move(request),
    *context,imports);
  query::AsyncMessageCursor output;
  output.next=[sourceCursor,context,read]()mutable{return sourceCursor.next();};
  output.close=[sourceCursor,context,read]()mutable{return sourceCursor.close();};
  return output;
}
} // namespace batbox::application_media
