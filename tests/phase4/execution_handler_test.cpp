#include "execution/GadgetExecution.hpp"
#include <cassert>
#include <future>
#include <iostream>

using batbox::Json;
using namespace batbox;

template<class T> std::future<T> ready(T value){std::promise<T> p;p.set_value(std::move(value));return p.get_future();}
struct FakeMcpAuthError:std::runtime_error{
 std::string serverName;
 FakeMcpAuthError():std::runtime_error("authorization required"),serverName("server"){}
};

int main(){
 gadgets::Gadget gadget;
 gadget.name="FileRead";
 gadget.parseInput=[](const Json& input){return input;};
 gadget.backfillObservableInput=[](Json& input){input["file_path"]="/absolute/file.txt";};
 Json calledInput;
 gadget.call=[&](const Json& input,const gadgets::GadgetCallContext& context,
   const std::function<std::future<Json>(const Json&)>&,
   const Json&,const std::function<void(const gadgets::GadgetProgress&)>& progress){
    assert(!context.stopToken.stop_requested());
    calledInput=input;
    progress(gadgets::GadgetProgress{"tool-1",Json{{"type","stdout"},{"text","working"}}});
    gadgets::GadgetResult result;
    result.data=Json{{"content","done"}};
    result.structuredOutput=Json{{"ok",true}};
    result.presentationMetadata=Json{{"memoryMtimeMs",12345}};
    result.newMessages=std::vector<Json>{Json{{"type","attachment"},{"attachment",{{"type","extra"}}}}};
    return ready(std::move(result));
  };
 gadget.mapGadgetResultWithPresentationMetadata=[](const Json& data,const std::string& id,
   const std::optional<Json>& metadata){
   assert(metadata && metadata->at("memoryMtimeMs")==12345);
   return Json{{"type","tool_result"},{"content",data.at("content")},
     {"tool_use_id",id},{"presentationMetadata",*metadata}};
 };
 execution::Context context;
 context.data={{"messages",Json::array()},{"agentId",nullptr}};
 context.gadgets={&gadget};
 std::stop_source stop;context.stop=stop.get_token();
 execution::Services services;
 services.createUserMessage=[](const Json& args){return Json{{"type","user"},{"args",args}};};
 services.createAttachmentMessage=[](const Json& att){return Json{{"type","attachment"},{"attachment",att}};};
 services.createProgressMessage=[](const std::string& id,const Json& progress){
   return Json{{"type","progress"},{"parentToolUseID",id},{"progress",progress}};};
 services.preGadgetUseHooks=[](const gadgets::Gadget&,const Json& input,const Json&,
   const execution::Context&,const std::function<void(const Json&)>&){
    assert(input.at("file_path")=="/absolute/file.txt");
   };
 services.resolvePermissionDecision=[](const Json&,const gadgets::Gadget&,const Json& input,
   const execution::Context&,const Json&,const std::string&){
    return Json{{"decision",{{"behavior","allow"}}},{"input",input}};
   };
 services.postGadgetUseHooks=[](const gadgets::Gadget&,const Json&,const Json&,
   const execution::Context&,const std::function<void(const Json&)>&){};
 services.processPreMappedGadgetResultBlock=[](const Json& block,const gadgets::Gadget&){return block;};
 services.canUseGadget=[](const Json&){return ready(Json{{"behavior","allow"}});};
 services.addToGadgetDuration=[](double){};
 services.nowMillis=[]{return std::int64_t{100};};
 Json emitted=Json::array();
 const Json assistant={{"type","assistant"},{"uuid","assistant-uuid"},
   {"message",{{"id","message-1"},{"content",Json::array()}}}};
 const Json block={{"type","tool_use"},{"id","tool-1"},{"name","FileRead"},
   {"input",{{"file_path","relative/file.txt"}}}};
 execution::runGadgetUse(block,assistant,context,services,[&](const execution::Update& update){
   if(update.message)emitted.push_back(*update.message);
 });
 assert(calledInput.at("file_path")=="relative/file.txt");
 assert(emitted.size()==4);
 assert(emitted[0]["type"]=="progress");
 assert(emitted[1]["attachment"]["type"]=="structured_output");
 assert(emitted[2]["type"]=="user");
 assert(emitted[2]["args"]["content"][0]["content"]=="done");
 assert(emitted[2]["args"]["content"][0]["presentationMetadata"]["memoryMtimeMs"]==12345);
 assert(emitted[3]["attachment"]["type"]=="extra");

 services.resolvePermissionDecision=[](const Json&,const gadgets::Gadget&,const Json& input,
   const execution::Context&,const Json&,const std::string&){
    return Json{{"decision",{{"behavior","deny"},{"message","blocked"}}},{"input",input}};
   };
 emitted=Json::array();calledInput=nullptr;
 execution::runGadgetUse(block,assistant,context,services,[&](const execution::Update& update){
   if(update.message)emitted.push_back(*update.message);
 });
 assert(calledInput.is_null());
 assert(emitted.size()==1);
 assert(emitted[0]["args"]["content"][0]["content"]=="blocked");

 services.resolvePermissionDecision=[](const Json&,const gadgets::Gadget&,const Json& input,
   const execution::Context&,const Json&,const std::string&){
    return Json{{"decision",{{"behavior","allow"}}},{"input",input}};
 };
 gadget.call=[](const Json&,const gadgets::GadgetCallContext&,
   const std::function<std::future<Json>(const Json&)>&,const Json&,
   const std::function<void(const gadgets::GadgetProgress&)>&)->std::future<gadgets::GadgetResult>{
   throw FakeMcpAuthError();
 };
 services.mcpAuthServerName=[](const std::exception& error)->std::optional<std::string>{
   if(const auto* auth=dynamic_cast<const FakeMcpAuthError*>(&error))return auth->serverName;
   return std::nullopt;
 };
 Json appState={{"mcp",{{"clients",Json::array({
   Json{{"name","server"},{"type","connected"},{"config",{{"url","local"}}}},
   Json{{"name","other"},{"type","connected"},{"config",Json::object()}}
 })}}}};
 services.updateAppState=[&](const std::function<Json(const Json&)>& updater){appState=updater(appState);};
 services.formatError=[](const std::exception& error){return std::string(error.what());};
 services.isAbortError=[](const std::exception&){return false;};
 services.postGadgetUseFailureHooks=[](const gadgets::Gadget&,const Json&,const std::string&,
   bool,const execution::Context&,const std::function<void(const Json&)>&){};
 services.mcpMetaFromError=[](const std::exception&)->std::optional<Json>{return std::nullopt;};
 context.gadgets={&gadget};emitted=Json::array();
 execution::runGadgetUse(block,assistant,context,services,[&](const execution::Update& update){
   if(update.message)emitted.push_back(*update.message);
 });
 assert(appState["mcp"]["clients"][0]["type"]=="needs-auth");
 assert(appState["mcp"]["clients"][0]["config"]["url"]=="local");
 assert(appState["mcp"]["clients"][1]["type"]=="connected");
 assert(emitted.size()==1&&emitted[0]["args"]["content"][0]["is_error"]==true);
 appState["mcp"]["clients"][0]["type"]="disconnected";
 execution::runGadgetUse(block,assistant,context,services,[](const execution::Update&){});
 assert(appState["mcp"]["clients"][0]["type"]=="disconnected");
 std::cout<<"actual tool handler progress, backfill isolation, structured output, permission and MCP auth state passed\n";
}
