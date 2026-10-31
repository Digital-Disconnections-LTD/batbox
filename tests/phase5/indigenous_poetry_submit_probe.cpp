#include "interactive/submit/PoetrySubmit.hpp"
#include "interactive/submit/Workload.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace sub=batbox::interactive::submit;
using Out=nlohmann::ordered_json;
static Out* events=nullptr;

namespace batbox::user_input {
event_loop::Deferred<InputResult> processUserInput(
    const UserInputRequest& request,UserInputServices& services){
  event_loop::Deferred<InputResult> output(services.loop);
  const auto input=request.input.is_string()?request.input.get<std::string>():"";
  events->push_back(Out::array({"process",input,request.isAlreadyProcessing,
    request.skipAttachments,!request.pastedContents.is_null()&&
      !request.pastedContents.is_discarded(),
    !request.ideSelection.is_null()&&!request.ideSelection.is_discarded(),
    sub::getWorkload()?Out(*sub::getWorkload()):Out(nullptr)}));
  if(request.mode=="prompt"&&request.input.is_string()&&!request.isMeta&&
     services.setUserInputOnProcessing)
    services.setUserInputOnProcessing(input);
  if(input=="throw"){
    output.reject(std::make_exception_ptr(std::runtime_error("failed input")));
    return output;
  }
  InputResult result;
  if(input=="local"){
    result.nextInput="draft";output.resolve(std::move(result));return output;
  }
  result.messages=Out::array({Out{{"type","user"},
    {"uuid",request.uuid.value_or(input)},
    {"message",Out{{"content",input}}}}});
  result.shouldQuery=true;
  result.allowedGadgets=Out::array({"Read"});
  if(input=="query")result.model="skill";
  result.effort="high";
  if(input=="query")result.nextInput="follow";
  output.resolve(std::move(result));return output;
}
} // namespace batbox::user_input

static std::shared_ptr<sub::SubmitServices> makeServices(
    event_loop::Loop& loop,interactive::QueryGuard& guard,
    sub::CommandQueue& queue,
    const std::vector<user_input::CommandEntry>& catalog,
    messages::Services& messageServices,user_input::SlashServices& slash,
    bool& historyEnabled){
  auto services=std::make_shared<sub::SubmitServices>(
    sub::SubmitServices{loop,guard,queue,catalog});
  services->makeUserInputServices=[&](const std::shared_ptr<interactive::TurnAbort>&,
                                     const std::string&){
    events->push_back(Out::array({"context"}));
    return std::make_shared<user_input::UserInputServices>(
      user_input::UserInputServices{loop,messageServices,slash});
  };
  services->onQuery=[&](interactive::TurnRequest query){
    event_loop::Deferred<event_loop::Unit> pending(loop);
    guard.tryStart();events->push_back(Out::array({"start","running"}));
    Out messages=Out::array();
    for(const auto& item:*query.newMessages){
      const auto& origin=value::property(*item,"origin");
      const auto& kind=value::property(origin,"kind");
      messages.push_back(Out::array({value::property(*item,"uuid"),
        kind.is_string()?Out(kind):Out(nullptr)}));
    }
    Out gadgets=Out::array();
    for(const auto& gadget:query.additionalAllowedGadgets)gadgets.push_back(gadget);
    events->push_back(Out::array({"query",messages,query.shouldQuery,gadgets,
      query.mainLoopModel,static_cast<bool>(query.onBeforeQuery),
      query.input?Out(*query.input):Out(nullptr),
      query.effort.is_discarded()?Out(nullptr):query.effort,
      sub::getWorkload()?Out(*sub::getWorkload()):Out(nullptr)}));
    guard.end(guard.generation());
    events->push_back(Out::array({"end","idle"}));
    pending.resolve({});return pending;
  };
  services->invokeImmediateUi=[&](const user_input::CommandEntry&,
      const std::string&,sub::ImmediateContext context,
      std::function<void(const interactive::CommandCompletion&)> onDone){
    event_loop::Deferred<event_loop::Unit> pending(loop);
    if(!context.abort||context.mainLoopModel!="main")
      throw std::logic_error("immediate context required");
    events->push_back(Out::array({"context"}));
    events->push_back(Out::array({"immediateCall"}));
    interactive::CommandCompletion completion;
    completion.result="done";
    completion.options.nextInput="next";
    completion.options.submitNextInput=true;
    onDone(completion);pending.resolve({});return pending;
  };
  services->isCommandEnabled=[](const Json&){return true;};
  services->gracefulShutdownSync=[](int code){
    events->push_back(Out::array({"exit",code}));};
  services->notifyImmediate=[](const std::string&,const std::string&){};
  services->logForDebugging=[](const std::string& text){
    events->push_back(Out::array({"debug",text}));};
  services->queryCheckpoint=[&](const std::string& name){
    events->push_back(Out::array({"checkpoint",name,guard.isActive()}));};
  services->startQueryProfile=[]{events->push_back(Out::array({"profile"}));};
  services->clearLocalUi=[&]{
    events->push_back(Out::array({"clearUi",guard.isActive()}));};
  services->setUserInputOnProcessing=[](std::optional<std::string> value){
    events->push_back(Out::array({"placeholder",value?Out(*value):Out(nullptr)}));};
  services->setAbortController=[](std::shared_ptr<interactive::TurnAbort> value){
    events->push_back(Out::array({"controller",static_cast<bool>(value)}));};
  services->fileHistoryEnabled=[&]{return historyEnabled;};
  services->selectableUserMessage=[](const interactive::TurnMessageRef& item){
    return value::property(*item,"type")=="user";};
  services->fileHistoryMakeSnapshot=[](const Json& uuid){
    events->push_back(Out::array({"snapshot",uuid}));};
  services->resolveSkillModelOverride=[](const std::string& model,
                                         const std::string& main){
    events->push_back(Out::array({"model",model,main}));
    return model+":resolved";};
  services->onDetachedError=[](std::exception_ptr error){
    std::rethrow_exception(error);};
  return services;
}
static sub::PoetrySubmitRequest request(){
  sub::PoetrySubmitRequest result;
  result.mainLoopModel="main";result.querySource="repl";
  result.ideSelection=Out{{"file","a"}};
  result.helpers.setCursorOffset=[](std::size_t offset){
    events->push_back(Out::array({"caret",offset}));};
  result.helpers.clearBuffer=[]{events->push_back(Out::array({"clearBuffer"}));};
  result.helpers.resetHistory=[]{events->push_back(Out::array({"historyReset"}));};
  result.helpers.onInputChange=[](const std::string& value){
    events->push_back(Out::array({"inputChange",value}));};
  result.helpers.setPastedContents=[](const Json& value){
    events->push_back(Out::array({"pasted",value.size()}));};
  return result;
}
int main(){
  event_loop::Loop loop;
  interactive::QueryGuard guard;
  Out trace=Out::array();events=&trace;
  std::shared_ptr<interactive::TurnAbort> currentAbort;
  sub::CommandQueue queue(loop,{[&](const std::string& op,
      const std::optional<std::string>& content){
    if(op=="enqueue")events->push_back(Out::array({
      "enqueue",content?Out(*content):Out(nullptr),
      currentAbort&&currentAbort->aborted()}));
  }});
  std::vector<user_input::CommandEntry> catalog(1);
  catalog[0].descriptor=Out{{"name","config"},{"immediate",true},
    {"type","local-jsx"},{"aliases",Out::array({"cfg"})}};
  messages::Services messageServices;
  user_input::SlashServices slash{loop,messageServices,catalog};
  bool historyEnabled=false;
  auto services=makeServices(loop,guard,queue,catalog,messageServices,
                             slash,historyEnabled);
  auto run=[&](const std::string& name,sub::PoetrySubmitRequest poetry,
               bool busy=false,bool history=false){
    guard.forceEnd();queue.reset();trace=Out::array();
    historyEnabled=history;
    if(busy){guard.tryStart();trace=Out::array();}
    currentAbort=poetry.activeAbort;
    auto pending=sub::handlePoetrySubmit(std::move(poetry),services);
    pending.then([&](const event_loop::Unit*,const std::exception_ptr& error){
      if(error){
        try{std::rethrow_exception(error);}
        catch(const std::exception& failure){
          events->push_back(Out::array({"rejected",failure.what()}));}
      }else events->push_back(Out::array({"resolved"}));
    });
    loop.runReady();
    std::cout<<Out{{"name",name},{"events",trace}}.dump()<<'\n';
  };
  auto direct=request();direct.input="query";
  run("direct-query",direct);
  auto batch=request();
  batch.queuedCommands=sub::CommandList{
    std::make_shared<Json>(Out{{"value","first"},
      {"mode","task-notification"},{"uuid","u1"},{"workload","cron"}}),
    std::make_shared<Json>(Out{{"value","second"},
      {"mode","task-notification"},{"uuid","u2"},{"workload","cron"}})};
  run("queued-batch",batch);
  batch=request();
  batch.queuedCommands=sub::CommandList{
    std::make_shared<Json>(Out{{"value","first"},
      {"mode","prompt"},{"workload","cron"}}),
    std::make_shared<Json>(Out{{"value","second"},{"mode","prompt"}})};
  run("mixed-workload",batch);
  direct=request();direct.input="query";run("file-history",direct,false,true);
  direct=request();direct.input="local";run("local-no-messages",direct);
  direct=request();direct.input="throw";run("process-error",direct);
  direct=request();direct.input="  hello  ";
  direct.hasInterruptibleGadgetInProgress=true;direct.streamMode="tool-use";
  direct.activeAbort=std::make_shared<interactive::TurnAbort>();
  run("busy-queue",direct,true);
  direct=request();direct.input="/cfg arg";
  run("immediate-early-done",direct,true);
}
