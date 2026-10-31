#include "interactive/submit/PoetrySubmit.hpp"
#include "interactive/submit/Workload.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace sub=batbox::interactive::submit;
static void expect(bool okay,const char* message){
  if(!okay)throw std::runtime_error(message);
}
int main(){
  event_loop::Loop loop;
  interactive::QueryGuard guard;
  sub::CommandQueue queue(loop);
  std::vector<user_input::CommandEntry> catalog;
  messages::Services messageServices;
  int nextUuid=0;
  messageServices.randomUUID=[&]{return "generated-"+std::to_string(++nextUuid);};
  messageServices.nowISO=[]{return "2026-10-29T17:00:00.000Z";};
  user_input::SlashServices slash{loop,messageServices,catalog};
  int attachmentCalls=0,queries=0,placeholderClears=0;
  std::vector<std::string> contents;
  auto services=std::make_shared<sub::SubmitServices>(
    sub::SubmitServices{loop,guard,queue,catalog});
  services->makeUserInputServices=[&](
      const std::shared_ptr<interactive::TurnAbort>& abort,
      const std::string& model){
    expect(abort&&!abort->aborted()&&model=="main","query context");
    auto input=std::make_shared<user_input::UserInputServices>(
      user_input::UserInputServices{loop,messageServices,slash});
    input->getAppState=[]{return Json{{"toolPermissionContext",
      Json{{"mode","default"}}}};};
    input->feature=[](const std::string&){return false;};
    input->getAttachmentMessages=[&](const std::string&,const Json&,
        const Json&,const Json&,const std::optional<std::string>&){
      ++attachmentCalls;
      event_loop::Deferred<Json> done(loop);done.resolve(Json::array());return done;
    };
    input->executeUserPoetrySubmitHooks=[&](const std::string&,
                                             const std::string&,std::stop_token){
      return instructions::hooks::runtime::HookEventGenerator(loop,
        []()->std::optional<instructions::hooks::runtime::ExecuteHooksInput>{
          return std::nullopt;},
        [](instructions::hooks::runtime::ExecuteHooksInput){
          return instructions::hooks::runtime::HookYieldCursor{};});
    };
    return input;
  };
  services->onQuery=[&](interactive::TurnRequest request){
    event_loop::Deferred<event_loop::Unit> done(loop);
    const auto generation=guard.tryStart();
    expect(generation.has_value(),"query guard reserved");
    ++queries;
    for(const auto& message:*request.newMessages){
      expect(value::property(*message,"type")=="user","actual user message");
      const auto& content=value::property(value::property(*message,"message"),"content");
      expect(content.is_string(),"actual text content");
      contents.push_back(content.get<std::string>());
    }
    expect(request.abort&&!request.abort->aborted(),"query abort identity");
    guard.end(*generation);done.resolve({});return done;
  };
  services->setUserInputOnProcessing=[&](std::optional<std::string> value){
    if(!value)++placeholderClears;};
  services->setAbortController=[](std::shared_ptr<interactive::TurnAbort>){};
  services->startQueryProfile=[]{};
  services->queryCheckpoint=[](const std::string&){};
  services->fileHistoryEnabled=[]{return false;};
  services->clearLocalUi=[]{};
  sub::PoetrySubmitRequest direct;
  direct.input="hello";direct.mainLoopModel="main";
  direct.querySource="repl";
  direct.helpers.resetHistory=[]{};
  direct.helpers.onInputChange=[](const std::string&){};
  auto first=sub::handlePoetrySubmit(direct,services);
  bool firstDone=false;
  first.then([&](const event_loop::Unit*,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    firstDone=true;
  });
  loop.runReady();
  expect(firstDone&&queries==1&&contents==std::vector<std::string>{"hello"},
         "direct native user input to query");
  sub::PoetrySubmitRequest batch=direct;
  batch.input.reset();
  batch.queuedCommands=sub::CommandList{
    std::make_shared<Json>(Json{{"value","one"},{"mode","prompt"}}),
    std::make_shared<Json>(Json{{"value","two"},{"mode","prompt"}})};
  auto second=sub::handlePoetrySubmit(batch,services);
  bool secondDone=false;
  second.then([&](const event_loop::Unit*,const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    secondDone=true;
  });
  loop.runReady();
  expect(secondDone&&queries==2&&attachmentCalls==2&&placeholderClears==2,
         "batched native input and first-only attachments");
  expect(contents==std::vector<std::string>({"hello","one","two"}),
         "batched user message order");
  std::cout<<"PASS actual processUserInput -> submit -> query, "
    <<contents.size()<<" messages\n";
}
