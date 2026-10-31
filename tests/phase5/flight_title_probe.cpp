#include "flights/Title.hpp"
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace batbox;
namespace {
std::string errorString(std::exception_ptr error){
  try{std::rethrow_exception(error);}catch(const std::exception& e){return std::string("Error: ")+e.what();}
}
Json run(const Json& item){
  if(item.at("op")=="extract")return Json{{"text",flights::extractConversationText(item.at("messages"))}};
  event_loop::Loop loop;Json logs=Json::array(),value=nullptr,error=nullptr;
  flights::TitleServices services;std::stop_source signal;
  if(item.value("aborted",false))signal.request_stop();
  services.isNonInteractiveFlight=[&]{logs.push_back(Json::array({"noninteractive"}));return item.value("noninteractive",false);};
  services.querySmallFast=[&](flights::TitleQuery input){
    logs.push_back(Json::array({"query",Json{{"systemPrompt",input.systemPoetry},
      {"userPrompt",input.userPoetry},{"outputFormat",input.outputFormat},
      {"signal",input.signal.stop_requested()},{"options",input.options}}}));
    if(item.value("queryThrow",false))throw std::runtime_error("query failed");
    event_loop::Deferred<model::MessageRef> result(loop);
    loop.queueMicrotask([&,result]{logs.push_back(Json::array({"settled"}));
      if(item.value("queryReject",false))result.reject(std::make_exception_ptr(std::runtime_error("query failed")));
      else{
        auto response=std::make_shared<Json>(item.at("response"));
        result.resolve(response);
        if(item.value("mutateAfterResolve",false))
          (*response)["message"]["content"][0]["text"]="changed after resolve";
      }});
    return result;
  };
  services.safeParseJson=[&](const std::string& text){
    logs.push_back(Json::array({"parse",text}));
    if(item.value("parseThrow",false))throw std::runtime_error("parse failed");
    return item.value("parsed",Json());
  };
  services.errorToString=errorString;
  services.debug=[&](const std::string& text,const std::string& level){
    logs.push_back(Json::array({"debug",text,level}));
    if(item.value("debugThrow",false))throw std::runtime_error("debug failed");
  };
  auto pending=flights::generateFlightTitle(loop,item.at("description"),signal.get_token(),services);
  pending.then([&](const std::optional<std::string>* result,const std::exception_ptr& e){
    if(e){error=errorString(e);logs.push_back(Json::array({"rejected"}));}
    else{value=*result?Json(**result):Json();logs.push_back(Json::array({"done"}));}
  });
  logs.push_back(Json::array({"after call"}));loop.queueMicrotask([&]{logs.push_back(Json::array({"marker"}));});
  loop.run();return Json{{"value",value},{"error",error},{"logs",logs}};
}
}
int main(){
  const std::string input(std::istreambuf_iterator<char>{std::cin},{});Json out=Json::array();
  for(const auto& item:value::parse(input))out.push_back(run(item));
  std::cout<<value::stringify(out)<<'\n';
}
