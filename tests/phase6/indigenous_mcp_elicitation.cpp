#include "application_mcp_elicitation/Controller.hpp"
#include "application_mcp_elicitation/Validation.hpp"
#include "mcp/StdioClient.hpp"
#include "runtime/JsonValue.hpp"
#include <chrono>
#include <future>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <thread>

using namespace batbox;
namespace {
void check(bool value,const char* reason){if(!value)throw std::logic_error(reason);}
void send(const Json& value){std::cout<<value.dump()<<'\n'<<std::flush;}
int server(){
  std::string line;
  Json gadgetId;
  while(std::getline(std::cin,line)){
    const auto message=Json::parse(line);
    const auto& method=value::property(message,"method");
    if(method=="initialize"){
      const auto& caps=value::property(value::property(message,"params"),"capabilities");
      if(!value::property(value::property(caps,"elicitation"),"form").is_object()||
          !value::property(value::property(caps,"elicitation"),"url").is_object())return 3;
      send({{"jsonrpc","2.0"},{"id",message.at("id")},
        {"result",{{"protocolVersion","2025-11-25"},
          {"capabilities",{{"tools",Json::object()}}},
          {"serverInfo",{{"name","test"},{"version","1"}}}}}});
    }else if(method=="tools/call"){
      gadgetId=message.at("id");
      const auto name=value::property(value::property(message,"params"),"name");
      if(name=="url")send({{"jsonrpc","2.0"},{"id","ask-url"},
        {"method","elicitation/create"},
        {"params",{{"mode","url"},{"message","Open test flow"},
          {"url","https://example.com/confirm"},{"elicitationId","flow-1"}}}});
      else send({{"jsonrpc","2.0"},{"id","ask-form"},
        {"method","elicitation/create"},
        {"params",{{"mode","form"},{"message","Fill test fields"},
          {"requestedSchema",{{"type","object"},
            {"properties",{{"email",{{"type","string"},{"format","email"}}},
              {"count",{{"type","integer"},{"minimum",1}}}}},
            {"required",Json::array({"email","count"})}}}}}});
      send({{"jsonrpc","2.0"},{"method","testing/pulse"}});
    }else if(message.contains("id")&&
        (message.at("id")=="ask-url"||message.at("id")=="ask-form")){
      const auto& result=value::property(message,"result");
      if(message.at("id")=="ask-url")send({{"jsonrpc","2.0"},
        {"method","notifications/elicitation/complete"},
        {"params",{{"elicitationId","flow-1"}}}});
      send({{"jsonrpc","2.0"},{"id",gadgetId},
        {"result",{{"content",Json::array({{{"type","text"},
          {"text",result.dump()}}})}}}});
    }
  }
  return 0;
}
int validation(){
  std::string line;
  while(std::getline(std::cin,line)){
    const auto input=Json::parse(line);
    const auto& schema=input.at("schema");
    Json result;
    if(input.at("kind")=="options"){
      result=Json::array();
      for(const auto& option:application_mcp_elicitation::optionsFor(schema))
        result.push_back({{"value",option.value},{"label",option.label}});
    }else{
      std::string problem;
      auto value=application_mcp_elicitation::validateInput(
        input.at("raw").get<std::string>(),schema,true,problem);
      result={{"isValid",value.has_value()}};
      if(value)result["value"]=*value;
    }
    send(result);
  }
  return 0;
}
template<class Predicate>void until(event_loop::Loop& loop,Predicate ready){
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
  while(!ready()){
    loop.runReady();
    if(std::chrono::steady_clock::now()>deadline)
      throw std::logic_error("Timed out waiting for MCP elicitation");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  loop.runReady();
}
}
int main(int argc,char** argv){
  if(argc>1&&std::string(argv[1])=="--server")return server();
  if(argc>1&&std::string(argv[1])=="--validation")return validation();
  std::string error;
  check(application_mcp_elicitation::validateInput("mailto:user@example.com",
      {{"type","string"},{"format","uri"}},true,error).has_value(),
    "WHATWG URI accepts mailto scheme");
  check(application_mcp_elicitation::validateInput(
      "2026-10-30T12:30:00-04:00",{{"type","string"},{"format","date-time"}},
      true,error).has_value(),"Signed ISO offset must validate");
  check(!application_mcp_elicitation::validateInput("1e30",
      {{"type","integer"}},true,error),"Unsafe integer must be rejected");
  check(application_mcp_elicitation::validateInput(" 0x10 ",
      {{"type","integer"}},true,error)==Json(16),
    "JS number coercion must accept hexadecimal input");
  check(application_mcp_elicitation::validateInput("é",
      {{"type","string"},{"minLength",1},{"maxLength",1}},true,error).has_value(),
    "String length must count UTF-16 units rather than UTF-8 bytes");
  const auto choices=application_mcp_elicitation::optionsFor(
    {{"type","string"},{"oneOf",Json::array({{{"const","internal"},
      {"title","Internal label"}}})}});
  check(choices.size()==1&&choices[0].value=="internal"&&
      choices[0].label=="Internal label","Enum wire value must differ from title");

  event_loop::Loop loop;
  std::optional<interactive::UiRequest> visible;
  int pulses=0,opens=0;
  auto ui=std::make_shared<interactive::CommandUi>(loop,
    interactive::CommandUi::Callbacks{
      [&](auto value){visible=std::move(value);},
      [](const auto&,const auto&,bool){},
      [](const auto&){},[](bool){},[](bool){}});
  application_mcp_elicitation::Inputs inputs(loop);
  inputs.openUrl=[&](const std::string& url){
    check(url=="https://example.com/confirm","Wrong URL opened");++opens;};
  int dateQueries=0;
  inputs.parseNaturalDateTime=[&](const std::string& raw,
      const std::string& format,std::stop_token){
    check(format=="date","Natural date query selected wrong format");
    ++dateQueries;
    event_loop::Deferred<application_mcp_elicitation::DateTimeParseResult> answer(loop);
    if(raw=="tomorrow")answer.resolve({true,"2026-10-31",{}});
    else answer.resolve({false,{},"Unable to parse date/time from input"});
    return answer;
  };
  auto controller=application_mcp_elicitation::Controller::create(std::move(inputs));
  controller->attachUi(ui);
  mcp::ClientHandlers handlers;
  handlers.requestAsync=[dispatcher=loop.dispatcher(),controller](
      const Json& request,std::stop_token signal,
      std::function<void(Json,std::exception_ptr)> complete){
    dispatcher.postMission([controller,request,signal,complete=std::move(complete)]{
      controller->request("fixture",request,signal).then(
        [complete](const Json* value,const std::exception_ptr& failure){
          if(failure)complete({},failure);
          else complete(value?*value:Json{{"action","cancel"}},{});
        });
    });
  };
  handlers.notification=[dispatcher=loop.dispatcher(),controller,&pulses](
      const Json& notification){
    dispatcher.postMission([controller,notification,&pulses]{
      if(value::property(notification,"method")=="testing/pulse")++pulses;
      if(value::property(notification,"method")=="notifications/elicitation/complete")
        controller->complete("fixture",value::property(
          value::property(notification,"params"),"elicitationId").get<std::string>());
    });
  };
  auto client=std::make_shared<mcp::StdioClient>(
    std::vector<std::string>{argv[0],"--server"},"fixture",
    std::chrono::milliseconds(5000),
    std::map<std::string,std::string>{},std::nullopt,std::nullopt,handlers);
  auto url=std::async(std::launch::async,[client]{
    return client->invoke("tools/call",{{"name","url"},{"arguments",Json::object()}});
  });
  until(loop,[&]{return visible&&visible->id=="mcp-elicitation-url"&&pulses==1;});
  ui->dispatchAction({interactive::UiAction::Kind::Activate,0,{}});
  until(loop,[&]{return url.wait_for(std::chrono::milliseconds(0))==
    std::future_status::ready&&!visible;});
  check(opens==1&&url.get().dump().find("accept")!=std::string::npos,
    "URL accept and completion must round trip over live stdio reader");

  auto form=std::async(std::launch::async,[client]{
    return client->invoke("tools/call",{{"name","form"},{"arguments",Json::object()}});
  });
  until(loop,[&]{return visible&&visible->id=="mcp-elicitation-field"&&pulses==2;});
  // The JSON object retains schema insertion order.
  ui->dispatchAction({interactive::UiAction::Kind::TextChange,0,"user@example.com"});
  ui->dispatchAction({interactive::UiAction::Kind::Activate,0,{}});
  ui->dispatchAction({interactive::UiAction::Kind::TextChange,0,"2"});
  ui->dispatchAction({interactive::UiAction::Kind::Activate,0,{}});
  check(visible&&visible->id=="mcp-elicitation-confirm",
    "Form fields must advance to confirmation");
  ui->dispatchAction({interactive::UiAction::Kind::Activate,0,{}});
  until(loop,[&]{return form.wait_for(std::chrono::milliseconds(0))==
    std::future_status::ready&&!visible;});
  const auto response=Json::parse(form.get().at("content")[0].at("text").get<std::string>());
  check(response.at("action")=="accept"&&
      response.at("content").at("email")=="user@example.com"&&
      response.at("content").at("count")==2,
    "Form values must reach MCP server with typed integer");

  auto declined=controller->request("fixture",{{"method","elicitation/create"},
    {"params",{{"mode","url"},{"url","https://example.com"}}}});
  std::optional<Json> declineResult;
  declined.then([&](const Json* value,const std::exception_ptr&){if(value)declineResult=*value;});
  check(visible&&visible->id=="mcp-elicitation-url","Direct decline dialog missing");
  ui->dispatchAction({interactive::UiAction::Kind::Activate,1,{}});
  loop.runReady();
  check(declineResult&&value::property(*declineResult,"action")=="decline",
    "Decline must differ from cancellation");
  const Json dateRequest={{"method","elicitation/create"},
    {"params",{{"mode","form"},{"message","Pick a date"},
      {"requestedSchema",{{"properties",{{"when",{{"type","string"},
        {"format","date"}}}}},{"required",Json::array({"when"})}}}}}};
  auto natural=controller->request("fixture",dateRequest);
  std::optional<Json> naturalResult;
  natural.then([&](const Json* value,const std::exception_ptr&){
    if(value)naturalResult=*value;});
  ui->dispatchAction({interactive::UiAction::Kind::TextChange,0,"tomorrow"});
  ui->dispatchAction({interactive::UiAction::Kind::Activate,0,{}});
  check(visible&&visible->id=="mcp-elicitation-validating",
    "Non-ISO input must show asynchronous date parsing state");
  loop.runReady();
  check(dateQueries==1&&visible&&visible->id=="mcp-elicitation-confirm",
    "Configured date parser must advance a valid natural-language date");
  ui->dispatchAction({interactive::UiAction::Kind::Activate,0,{}});
  loop.runReady();
  check(naturalResult&&naturalResult->at("content").at("when")=="2026-10-31",
    "Parsed date must be revalidated and sent as ISO");
  auto invalidDate=controller->request("fixture",dateRequest);
  std::optional<Json> invalidResult;
  invalidDate.then([&](const Json* value,const std::exception_ptr&){
    if(value)invalidResult=*value;});
  ui->dispatchAction({interactive::UiAction::Kind::TextChange,0,"nonsense"});
  ui->dispatchAction({interactive::UiAction::Kind::Activate,0,{}});
  loop.runReady();
  check(dateQueries==2&&visible&&visible->id=="mcp-elicitation-field"&&
      visible->body.find("Must be a valid date")!=std::string::npos,
    "Invalid model date must keep the field open with validation error");
  ui->dispatchAction({interactive::UiAction::Kind::Cancel,0,{}});
  loop.runReady();
  check(invalidResult&&invalidResult->at("action")=="cancel",
    "Date parse failure must still permit explicit cancel");
  std::stop_source stopped;
  auto canceled=controller->request("fixture",{{"method","elicitation/create"},
    {"params",{{"mode","url"},{"url","https://example.com"}}}},
    stopped.get_token());
  std::optional<Json> cancelResult;
  canceled.then([&](const Json* value,const std::exception_ptr&){if(value)cancelResult=*value;});
  stopped.request_stop();loop.runReady();
  check(cancelResult&&value::property(*cancelResult,"action")=="cancel"&&!visible,
    "Abort must settle and close the active dialog");
  auto hook=controller->request("fixture",{{"method","elicitation/create"},
    {"params",{{"mode","url"},{"url","https://example.com"}}}}, {},
    application_mcp_elicitation::Hooks{[&](const auto&,const auto&,auto){
      event_loop::Deferred<Json> result(loop);result.resolve(Json{{"action","accept"}});
      return result;},[&](const auto&,const auto&,const auto&,auto){
      throw std::logic_error("Before hook response must skip result hook");
      return event_loop::Deferred<Json>(loop);}});
  std::optional<Json> hookResult;
  hook.then([&](const Json* value,const std::exception_ptr&){if(value)hookResult=*value;});
  loop.runReady();
  check(hookResult&&value::property(*hookResult,"action")=="accept"&&!visible,
    "Before hook must auto-resolve without UI");
  controller->cancelAll();client->shutdown();
  std::cout<<"native MCP elicitation checks passed\n";
}
