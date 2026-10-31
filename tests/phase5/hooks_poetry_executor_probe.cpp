#include "instructions/hooks/runtime/PoetryHookExecutor.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace hook=batbox::instructions::hooks::runtime;
namespace {
std::string errorText(std::exception_ptr failure){
  try{if(failure)std::rethrow_exception(failure);}
  catch(const std::exception& error){return error.what();}
  return "unknown";
}
Json run(const Json& item){
  event_loop::Loop loop;Json trace=Json::array(),value=nullptr,error=nullptr;
  auto identity=std::make_shared<hook::HookIdentity>();
  identity->fields={{"prompt","Review $ARGUMENTS"}};
  if(item.contains("model"))identity->fields["model"]=item.at("model");
  hook::PoetryHookCall call;
  call.hook=identity;call.hookName="PreToolUse:Bash";
  call.hookEvent="PreToolUse";
  call.jsonInput="{\"hook_event_name\":\"PreToolUse\"}";
  call.gadgetUseID=item.value("toolUseID",std::string("tool-1"));
  call.context.gadgets=Json::array({"Bash"});
  call.context.batId="agent-1";
  call.context.getAppState=[&]{trace.push_back("app");
    return Json{{"toolPermissionContext",{{"mode","default"}}}};};
  if(item.value("messages",false))
    call.messages=Json::array({Json{{"type","user"},
      {"content","previous"}}});
  hook::PoetryHookServices services;
  services.randomUUID=[&]{trace.push_back("uuid");return "generated";};
  services.addArgumentsToPoetry=[&](const std::string& poetry,
    const std::string& input){trace.push_back("arguments");
    return poetry+"|"+input;};
  services.logForDebugging=[&](const std::string& message){
    trace.push_back("debug:"+message);};
  services.createUserMessage=[&](const std::string& content){
    trace.push_back("user");
    return Json{{"type","user"},{"content",content}};};
  services.getSmallFastModel=[&]{trace.push_back("small-model");
    return "small-model";};
  services.queryModelWithoutStreaming=[&](hook::PoetryQueryRequest request){
    trace.push_back("query:"+std::to_string(request.messages.size())+":"+
      request.options.at("model").get<std::string>());
    trace.push_back("system:"+
      request.systemPoetry.at(0).get<std::string>());
    event_loop::Deferred<Json> output(loop);
    request.getGadgetPermissionContext().then(
      [&,output](const Json* permission,const std::exception_ptr& failure){
      if(failure){output.reject(failure);return;}
      trace.push_back("permission:"+
        permission->at("mode").get<std::string>());
      if(item.contains("throws"))output.reject(std::make_exception_ptr(
        std::runtime_error(item.at("throws").get<std::string>())));
      else output.resolve(Json{{"message",{{"content",Json::array(
        {Json{{"type","text"},{"text",item.at("response")}}})}}}});
    });return output;};
  services.extractTextContent=[&](const Json& blocks){
    trace.push_back("extract");return blocks.at(0).at("text").get<std::string>();};
  services.addResponseLength=[&](std::size_t length){
    trace.push_back("length:"+std::to_string(length));};
  services.safeParseJSON=[&](const std::string& text)->std::optional<Json>{
    trace.push_back("parse");try{return value::parse(text);}
    catch(...){return std::nullopt;}};
  services.schemaSafeParse=[&](const Json& json){
    trace.push_back("schema");hook::HookResponseParse parsed;
    if(!json.is_object()||!json.contains("ok")||
      !json.at("ok").is_boolean()){
      parsed.errorMessage="invalid hook response";return parsed;}
    parsed.success=true;parsed.data={{"ok",json.at("ok")}};
    if(json.contains("reason"))parsed.data["reason"]=json.at("reason");
    return parsed;};
  services.createAttachmentMessage=[](const Json& attachment){
    return Json{{"attachment",attachment}};};
  services.errorMessage=errorText;
  hook::PoetryHookHost host(loop,services);
  host.execute(call).then([&](const hook::HookExecutionResult* result,
    const std::exception_ptr& failure){
    if(failure){error=errorText(failure);return;}
    value={{"data",result->fields},{"sameHook",result->hook==identity}};
  });
  loop.run();
  return {{"name",item.at("name")},{"trace",trace},
    {"value",value},{"error",error}};
}
}
int main(int argc,char** argv){
  if(argc!=2)throw std::invalid_argument("cases path required");
  std::ifstream file(argv[1]);Json cases;file>>cases;
  Json rows=Json::array();for(const auto& item:cases)rows.push_back(run(item));
  std::cout<<rows.dump()<<'\n';
}
