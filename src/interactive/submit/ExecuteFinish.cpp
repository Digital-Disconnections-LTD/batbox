#include "interactive/submit/Execution.hpp"
#include <stdexcept>

namespace batbox::interactive::submit {
namespace {
void need(const auto& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Submit binding required: ")+name);
}
}
void ExecutionRun::finishInputs(){
  need(services->queryCheckpoint,"queryCheckpoint");
  services->queryCheckpoint("query_process_user_input_end");
  need(services->fileHistoryEnabled,"fileHistoryEnabled");
  if(services->fileHistoryEnabled()){
    services->queryCheckpoint("query_file_history_snapshot_start");
    need(services->selectableUserMessage,"selectableUserMessage");
    need(services->fileHistoryMakeSnapshot,"fileHistoryMakeSnapshot");
    for(const auto& message:*newMessages)
      if(services->selectableUserMessage(message))
        services->fileHistoryMakeSnapshot(value::property(*message,"uuid"));
    services->queryCheckpoint("query_file_history_snapshot_end");
  }
  need(request.helpers.resetHistory,"resetHistory");
  need(services->clearLocalUi,"clearLocalUi");
  if(!newMessages->empty()){
    request.helpers.resetHistory();
    services->clearLocalUi();
    const auto& primary=commands.empty()?Json():*commands.front();
    const auto& primaryMode=value::property(primary,"mode");
    const bool poetry=primaryMode.is_discarded()||primaryMode.is_null()||
      primaryMode=="prompt";
    const auto& primaryValue=value::property(primary,"value");
    TurnRequest query;
    query.newMessages=newMessages;
    query.abort=abort;
    query.shouldQuery=shouldQuery;
    query.additionalAllowedGadgets=allowedGadgets;
    query.mainLoopModel=request.mainLoopModel;
    if(model&&!model->empty()){
      need(services->resolveSkillModelOverride,"resolveSkillModelOverride");
      query.mainLoopModel=services->resolveSkillModelOverride(
        *model,request.mainLoopModel);
    }
    if(poetry)query.onBeforeQuery=services->onBeforeQuery;
    if(primaryValue.is_string())
      query.input=primaryValue.get<std::string>();
    if(effort)query.effort=*effort;
    need(services->onQuery,"onQuery");
    await(services->onQuery(std::move(query)),
      [self=shared_from_this()](const event_loop::Unit&){
        self->afterQuery();});
  }else{
    services->guard.cancelReservation();
    services->clearLocalUi();
    request.helpers.resetHistory();
    need(services->setAbortController,"setAbortController");
    services->setAbortController(nullptr);
    afterQuery();
  }
}
void ExecutionRun::afterQuery(){
  if(nextInput&& !nextInput->empty()){
    if(submitNextInput.value_or(false))
      services->queue.enqueue(Json{{"value",*nextInput},{"mode","prompt"}});
    else{
      need(request.helpers.onInputChange,"onInputChange");
      request.helpers.onInputChange(*nextInput);
    }
  }
  finish();
}
void ExecutionRun::finish(std::exception_ptr error){
  if(finished)return;
  finished=true;
  try{
    services->guard.cancelReservation();
    need(services->setUserInputOnProcessing,"setUserInputOnProcessing");
    services->setUserInputOnProcessing(std::nullopt);
  }catch(...){error=std::current_exception();}
  currentInputServices.reset();
  if(error)result.reject(error);else result.resolve({});
}
} // namespace batbox::interactive::submit
