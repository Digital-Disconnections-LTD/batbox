#include "model/SimpleQueries.hpp"
#include <iostream>
#include <iterator>
#include <stdexcept>
using namespace batbox;
namespace {
std::string errorString(std::exception_ptr error){
  try{std::rethrow_exception(error);}catch(const std::exception& e){return std::string("Error: ")+e.what();}
}
Json run(const Json& item){
  event_loop::Loop loop;Json logs=Json::array(),value=nullptr,error=nullptr;int id=0;bool same=false;
  auto response=std::make_shared<Json>(Json{{"type","assistant"},{"message",{
    {"content",Json::array({Json{{"type","text"},{"text","answer"}}})}}}});
  model::SimpleQueryServices services;
  services.messageServices.randomUUID=[&]{logs.push_back(Json::array({"uuid",++id}));
    if(id==item.value("builderThrowAt",0))throw std::runtime_error("uuid failed");
    return "uuid-"+std::to_string(id);};
  services.messageServices.nowISO=[&]{logs.push_back(Json::array({"date"}));return std::string("2026-10-28T12:00:00.000Z");};
  services.getSmallFastModel=[&]{logs.push_back(Json::array({"smallModel"}));
    if(item.value("smallThrow",false))throw std::runtime_error("small failed");
    return std::string("fast-model");};
  services.queryWithoutStreaming=[&](model::SimpleQueryRequest request){
    const auto& input=request.input;
    logs.push_back(Json::array({"request",Json{{"messages",input.messages},
      {"systemPrompt",input.systemPoetry},{"thinkingConfig",input.thinkingConfig},
      {"tools",input.gadgets},{"signal",request.signal.stop_requested()},{"options",input.options}},request.ownOptionKeys}));
    if(item.value("queryThrow",false))throw std::runtime_error("query failed");
    request.getGadgetPermissionContext().then([&](const Json* permission,const std::exception_ptr& e){
      if(e)std::rethrow_exception(e);
      logs.push_back(Json::array({"permission",*permission}));});
    event_loop::Deferred<model::MessageRef> result(loop);
    if(item.value("mode",std::string())=="sync")result.resolve(response);
    else loop.queueMicrotask([&,result]{
      logs.push_back(Json::array({"settled"}));
      if(item.value("queryReject",false))result.reject(std::make_exception_ptr(std::runtime_error("query failed")));
      else{result.resolve(response);if(item.value("mutateAfterResolve",false))(*response)["message"]["content"][0]["text"]="late";}
    });
    return result;
  };
  model::SimpleQueryInputs input;
  input.systemPoetry=item.value("systemPrompt",Json::array());
  input.userPoetry=item.value("userPrompt",std::string("hello"));
  input.options=item.value("options",Json::object());
  if(item.contains("outputFormat"))input.outputFormat=item.at("outputFormat");
  std::stop_source signal;if(item.value("aborted",false))signal.request_stop();input.signal=signal.get_token();
  auto pending=item.value("small",false)?model::querySmallFast(loop,input,services):model::queryWithModel(loop,input,services);
  pending.then([&](const model::MessageRef* result,const std::exception_ptr& e){
    if(e){error=errorString(e);logs.push_back(Json::array({"rejected"}));}
    else{value=**result;same=*result==response;logs.push_back(Json::array({"done"}));}});
  logs.push_back(Json::array({"after call"}));loop.queueMicrotask([&]{logs.push_back(Json::array({"marker"}));});
  loop.run();return Json{{"value",value},{"error",error},{"same",same},{"logs",logs}};
}
}
int main(){
  const std::string input(std::istreambuf_iterator<char>{std::cin},{});Json out=Json::array();
  for(const auto& item:value::parse(input))out.push_back(run(item));
  std::cout<<value::stringify(out)<<'\n';
}
