#include "user_input/BashInput.hpp"
#include "gadgets/bash/BashGadget.hpp"
#include <iostream>

using namespace batbox;
namespace {
Json project(const Json& message){
  Json result={{"type","user"},{"content",message.at("message").at("content")}};
  if(message.contains("isMeta"))result["isMeta"]=message.at("isMeta");
  return result;
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const auto x=value::parse(line);
    event_loop::Loop loop;
    messages::Services messages;
    messages.randomUUID=[]{return "id";};
    messages.nowISO=[]{return "now";};
    user_input::BashInputServices services{loop,messages,{},{},{},{},{},{}};
    Json trace=Json::array();
    services.isPowerShellGadgetEnabled=[x]{return value::property(x,"powerShellEnabled")==true;};
    services.resolveDefaultShell=[x]{const auto& value=value::property(x,"defaultShell");
      return value.is_string()?value.get<std::string>():std::string("bash");};
    services.showProgress=[&](const std::string&,const std::optional<Json>&){
      trace.push_back("progress");};
    services.clearProgress=[&]{trace.push_back("clear");};
    services.callShell=[&loop,x](bool,const Json&,
      const std::function<void(const Json&)>&){
      event_loop::Deferred<Json> result(loop);
      const auto& kind=value::property(x,"error");
      if(kind=="shell"||kind=="interrupt")
        result.reject(std::make_exception_ptr(bash_gadget::ShellError(
          x.value("stdout",std::string{}),x.value("stderr",std::string{}),
          1,kind=="interrupt")));
      else if(kind=="other")result.reject(std::make_exception_ptr(
        std::runtime_error("failed")));
      else if(value::property(x,"noData")==true)result.resolve(Json::object());
      else result.resolve(Json{{"data",Json{{"stdout",x.value("stdout",std::string{})},
        {"stderr",x.value("stderr",std::string{})}}}});
      return result;
    };
    services.processGadgetResultBlock=[&loop,x](bool,const Json& data,const std::string&){
      event_loop::Deferred<Json> result(loop);
      const auto& content=value::property(x,"mappedContent");
      result.resolve(Json{{"content",content.is_null()?value::property(data,"stdout"):content}});
      return result;
    };
    auto pending=user_input::processBashCommand(x.at("input").get<std::string>(),
      x.value("preceding",Json::array()),x.value("attachments",Json::array()),services);
    std::optional<Json> answer;
    pending.then([&](const user_input::InputResult* value,
      const std::exception_ptr& error){
      if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){
        answer=Json{{"error",e.what()}};}}
      else{
        Json items=Json::array();
        for(const auto& message:value->messages)items.push_back(project(message));
        answer=Json{{"messages",items},{"shouldQuery",value->shouldQuery},{"trace",trace}};
      }
    });
    for(int i=0;i<20&&!answer;++i)loop.runReady();
    std::cout<<value::stringify(answer.value_or(Json{{"error","unsettled"}}))<<'\n';
  }
}
