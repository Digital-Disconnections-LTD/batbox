#include "interactive/submit/Execution.hpp"
#include "interactive/submit/Workload.hpp"
#include <stdexcept>

namespace batbox::interactive::submit {
namespace {
void need(const auto& value,const char* name){
  if(!value)throw std::invalid_argument(std::string("Submit binding required: ")+name);
}
std::optional<std::string> stringField(const Json& object,const char* name){
  const auto& value=value::property(object,name);
  if(!value.is_string())return {};
  return value.get<std::string>();
}
bool boolField(const Json& object,const char* name){
  return value::truthy(value::property(object,name));
}
}
ExecutionRun::ExecutionRun(PoetrySubmitRequest request,CommandList commands,
    std::shared_ptr<SubmitServices> services):
    request(std::move(request)),commands(std::move(commands)),
    services(std::move(services)),result(this->services->loop),
    abort(std::make_shared<TurnAbort>()){}

std::optional<std::string> ExecutionRun::turnWorkload()const{
  if(commands.empty())return {};
  const auto& first=value::property(*commands.front(),"workload");
  if(!first.is_string())return {};
  for(const auto& command:commands)
    if(value::property(*command,"workload")!=first)return {};
  return first.get<std::string>();
}
void ExecutionRun::start(){
  try{
    if(!services->loop.isOwnerThread())
      throw std::logic_error("Input execution requires owner thread");
    need(services->setAbortController,"setAbortController");
    services->setAbortController(abort);
    services->guard.reserve();
    need(services->queryCheckpoint,"queryCheckpoint");
    services->queryCheckpoint("query_process_user_input_start");
    WorkloadScope scope(turnWorkload());
    step();
  }catch(...){finish(std::current_exception());}
}
void ExecutionRun::step(){
  if(index>=commands.size()){finishInputs();return;}
  const auto& command=*commands[index];
  const bool first=index==0;
  user_input::UserInputRequest input;
  input.input=value::property(command,"value");
  input.abort=abort->controller.get_token();
  input.preExpansionInput=stringField(command,"preExpansionValue");
  input.mode=stringField(command,"mode").value_or("prompt");
  input.pastedContents=first?value::property(command,"pastedContents"):Json();
  input.ideSelection=first?request.ideSelection:Json();
  input.messages=request.messages;
  input.uuid=stringField(command,"uuid");
  if(!request.querySource.empty())input.querySource=request.querySource;
  input.isAlreadyProcessing=!first;
  input.skipSlashCommands=boolField(command,"skipSlashCommands");
  input.bridgeOrigin=boolField(command,"bridgeOrigin");
  input.isMeta=boolField(command,"isMeta");
  input.skipAttachments=!first;
  need(services->makeUserInputServices,"makeUserInputServices");
  currentInputServices=services->makeUserInputServices(abort,
                                                     request.mainLoopModel);
  if(!currentInputServices)
    throw std::invalid_argument("User input services required");
  if(first){
    need(services->setUserInputOnProcessing,"setUserInputOnProcessing");
    const auto setter=services->setUserInputOnProcessing;
    currentInputServices->setUserInputOnProcessing=[setter](
        const std::string& value){setter(value);};
  }else currentInputServices->setUserInputOnProcessing={};
  auto pending=user_input::processUserInput(input,*currentInputServices);
  await(std::move(pending),[self=shared_from_this()](
      const user_input::InputResult& value){
    self->currentInputServices.reset();
    self->accept(value);
  });
}
void ExecutionRun::accept(const user_input::InputResult& value){
  const auto& command=*commands[index];
  Json origin=value::property(command,"origin");
  if(origin.is_null()||origin.is_discarded()){
    if(value::property(command,"mode")=="task-notification")
      origin=Json{{"kind","task-notification"}};
  }
  if(!value.messages.is_array())
    throw std::invalid_argument("User input messages array required");
  for(const auto& item:value.messages){
    auto message=std::make_shared<Json>(item);
    if(!origin.is_null()&&!origin.is_discarded()&&
       value::property(*message,"type")=="user")
      (*message)["origin"]=origin;
    newMessages->push_back(std::move(message));
  }
  if(index==0){
    shouldQuery=value.shouldQuery;
    allowedGadgets.clear();
    if(value.allowedGadgets&&value.allowedGadgets->is_array())
      for(const auto& gadget:*value.allowedGadgets)
        if(gadget.is_string())allowedGadgets.push_back(gadget.get<std::string>());
    model=value.model;effort=value.effort;
    nextInput=value.nextInput;submitNextInput=value.submitNextInput;
  }
  ++index;
  step();
}
event_loop::Deferred<event_loop::Unit> executeUserInput(
    PoetrySubmitRequest request,CommandList commands,
    std::shared_ptr<SubmitServices> services){
  if(!services)throw std::invalid_argument("Submit services required");
  auto run=std::make_shared<ExecutionRun>(std::move(request),
                                         std::move(commands),std::move(services));
  run->start();return run->result;
}
} // namespace batbox::interactive::submit
