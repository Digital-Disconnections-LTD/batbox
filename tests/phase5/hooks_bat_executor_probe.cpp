#include "instructions/hooks/runtime/BatHookExecutor.hpp"
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
  identity->fields={{"prompt","Verify $ARGUMENTS"}};
  if(item.contains("model"))identity->fields["model"]=item.at("model");
  hook::BatHookCall call;
  call.hook=identity;call.hookName="Stop";call.hookEvent="Stop";
  call.jsonInput="{\"hook_event_name\":\"Stop\"}";
  call.gadgetUseID=item.value("toolUseID",std::string("tool-1"));
  if(item.contains("agentId"))
    call.context.batId=item.at("agentId").get<std::string>();
  call.context.gadgets=Json::array({Json{{"name","Bash"}},
    Json{{"name","AgentTool"}},Json{{"name","StructuredOutput"}}});
  call.context.getAppState=[&]{trace.push_back("app");
    return Json{{"toolPermissionContext",{{"mode","default"},
      {"alwaysAllowRules",{{"session",Json::array({"Read(/existing)"})}}}}}};};
  hook::BatHookServices services;
  int uuid=0;
  services.randomUUID=[&]{trace.push_back("uuid");
    return "id-"+std::to_string(++uuid);};
  services.getTranscriptPath=[&]{trace.push_back("path");
    return "/tmp/main.jsonl";};
  services.getBatTranscriptPath=[&](const std::string& bat){
    trace.push_back("agent-path");return "/tmp/"+bat+".jsonl";};
  services.addArgumentsToPoetry=[&](const std::string& poetry,
    const std::string& input){trace.push_back("arguments");
    return poetry+"|"+input;};
  services.logForDebugging=[&](const std::string& message){
    trace.push_back("debug:"+message);};
  services.createUserMessage=[&](const std::string& content){
    trace.push_back("user");
    return Json{{"type","user"},{"content",content}};};
  services.createStructuredOutputGadget=[&]{trace.push_back("structured-tool");
    return hook::BatStructuredOutputGadget{
      Json{{"name","StructuredOutput"}},
      std::make_shared<gadgets::Gadget>()};};
  services.gadgetMatchesName=[&](const Json& gadget,const std::string& name){
    const auto gadgetName=gadget.at("name").get<std::string>();
    trace.push_back("match-tool:"+gadgetName);return gadgetName==name;};
  services.isBatDisallowedGadget=[](const std::string& name){
    return name=="AgentTool";};
  services.structuredOutputGadgetName="StructuredOutput";
  services.getSmallFastModel=[&]{trace.push_back("small-model");
    return "small-model";};
  services.registerStructuredOutputEnforcement=[&](const std::string& id){
    trace.push_back("register:"+id);
    if(item.contains("registerError"))throw std::runtime_error(
      item.at("registerError").get<std::string>());};
  services.clearFlightHooks=[&](const std::string& id){
    trace.push_back("clear:"+id);};
  services.hasPermissionsToUseGadget=[&loop](const Json&){
    event_loop::Deferred<Json> output(loop);output.resolve(Json::object());
    return output;};
  services.query=[&,item](hook::BatQueryRequest request){
    struct StreamState {
      hook::BatQueryRequest request;
      std::size_t index=0;
      bool started=false,closed=false;
    };
    auto state=std::make_shared<StreamState>();
    state->request=std::move(request);
    hook::BatMessageStream stream;
    stream.next=[&,state,item]{
      event_loop::Deferred<hook::GeneratorStep<Json>> output(loop);
      if(!state->started){
        state->started=true;
        trace.push_back("query:"+state->request.batId+":"+
          state->request.model);
        std::string gadgets;
        for(const auto& gadget:state->request.gadgets){
          if(!gadgets.empty())gadgets+=",";
          gadgets+=gadget.at("name").get<std::string>();
        }
        trace.push_back("tools:"+gadgets);
        trace.push_back("system:"+
          state->request.systemPoetry.at(0).get<std::string>());
        const auto appState=state->request.getAppState();
        const auto& permission=appState.at("toolPermissionContext");
        std::string rules;
        for(const auto& rule:permission.at("alwaysAllowRules").at("session")){
          if(!rules.empty())rules+=",";
          rules+=rule.get<std::string>();
        }
        trace.push_back("state:"+permission.at("mode").get<std::string>()+
          ":"+rules);
      }
      if(item.contains("queryError")){
        if(!state->closed){state->closed=true;trace.push_back("query-close");}
        output.reject(std::make_exception_ptr(std::runtime_error(
          item.at("queryError").get<std::string>())));
        return output;
      }
      const auto count=item.value("maxTurns",false)?std::size_t{50}:
        item.at("stream").size();
      if(state->index>=count){
        if(!state->closed){state->closed=true;trace.push_back("query-close");}
        output.resolve({true,{}});return output;
      }
      Json message=item.value("maxTurns",false)?Json{{"type","assistant"}}:
        item.at("stream").at(state->index);
      ++state->index;
      output.resolve({false,std::move(message)});return output;
    };
    stream.close=[&,state]{
      event_loop::Deferred<hook::GeneratorStep<Json>> output(loop);
      if(!state->closed){state->closed=true;trace.push_back("query-close");}
      output.resolve({true,{}});return output;};
    return stream;};
  services.handleMessageFromStream=[&](const Json& message,
    const std::function<void(std::size_t)>& add,
    const std::function<void(const Json&)>&){
    const auto type=message.at("type").get<std::string>();
    trace.push_back("handle:"+type);
    if(type=="stream_event")add(1);};
  services.addResponseLength=[&](std::size_t length){
    trace.push_back("length:"+std::to_string(length));};
  services.schemaSafeParse=[&](const Json& json){
    trace.push_back("schema");hook::HookResponseParse parsed;
    if(!json.is_object()||!json.contains("ok")||
      !json.at("ok").is_boolean())return parsed;
    parsed.success=true;parsed.data={{"ok",json.at("ok")}};
    if(json.contains("reason"))parsed.data["reason"]=json.at("reason");
    return parsed;};
  services.jsonStringify=[](const Json& json){return value::stringify(json);};
  services.createAttachmentMessage=[](const Json& attachment){
    return Json{{"attachment",attachment}};};
  services.errorMessage=errorText;
  hook::BatHookHost host(loop,services);
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
