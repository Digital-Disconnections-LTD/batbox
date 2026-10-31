#include "user_input/SlashDispatch.hpp"
#include "user_input/UiCommandRouter.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
user_input::InputResult run(event_loop::Loop& loop,
  user_input::SlashServices& services,const std::string& input){
  auto pending=user_input::processSlashCommand(input,Json::array(),Json::array(),
    Json::array(),std::nullopt,services);
  std::optional<user_input::InputResult> result;
  pending.then([&](const user_input::InputResult* value,
    const std::exception_ptr& error){
    if(error)std::rethrow_exception(error);
    result=*value;
  });
  for(int i=0;i<20&&!result;i++)loop.runReady();
  require(bool(result),"UI command did not complete");
  return *result;
}
}
int main(){
  event_loop::Loop loop;
  messages::Services messages;
  messages.randomUUID=[]{return "id";};
  messages.nowISO=[]{return "now";};
  std::vector<std::string> events;
  user_input::UiCommandRouter::Projection projection;
  projection.onUiChanged=[&](std::optional<interactive::UiRequest> value){
    events.push_back(value?"view":"clear");};
  projection.onError=[&](const std::string&){events.push_back("error");};
  projection.onQueueBlocked=[&](bool blocked){events.push_back(blocked?"blocked":"unblocked");};
  projection.onPoetryFocus=[&](bool focused){events.push_back(focused?"focus":"blur");};
  auto router=user_input::UiCommandRouter::create(loop,std::move(projection));
  std::vector<user_input::CommandEntry> entries(1);
  entries[0].descriptor={{"name","config"},{"type","local-jsx"}};
  entries[0].invokeUi=[router,command=entries[0].descriptor]
    (const std::string& args){
    return router->invoke(command,args,
      [](interactive::CommandUi::Done done,interactive::CommandUi::Ready ready,
         interactive::CommandUi::Error){
        interactive::CommandCompletion value;
        value.result="Done";
        value.options.display=interactive::DoneOptions::Display::System;
        value.options.metaMessages={"hidden"};
        value.options.nextInput="next";
        value.options.submitNextInput=true;
        done(value);
        interactive::UiRequest request;request.title="Late";
        ready(request,[](const interactive::UiAction&){});
      });
  };
  user_input::SlashServices services{loop,messages,entries};
  services.isFullscreen=[]{return false;};
  auto result=run(loop,services,"/config");
  require(result.messages.size()==4,"system completion message count");
  require(result.messages[0].at("isMeta")==true,"local command caveat");
  require(result.messages[1].at("subtype")=="local_command","command breadcrumb");
  require(result.messages[2].at("content")=="<local-command-stdout>Done</local-command-stdout>",
    "system completion output");
  require(result.messages[3].at("isMeta")==true,"meta completion visibility");
  require(result.nextInput=="next"&&result.submitNextInput==true,
    "next input must survive completion");
  require(!router->ui()->busy()&&!router->ui()->hasActiveLocalUi(),
    "early completion must clear local UI");
  for(const auto& event:events)require(event!="view","late ready displayed stale UI");
  std::cout<<"native slash UI completion and early-done ordering passed\n";
}
