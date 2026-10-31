#include "user_input/ProcessUserInput.hpp"
#include "messages/TextContent.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
Json project(const Json& message){
  Json out={{"type",value::property(message,"type")}};
  if(value::property(message,"type")=="user")
    out["content"]=message.at("message").at("content");
  else if(message.contains("content"))out["content"]=message.at("content");
  for(const auto* key:{"subtype","isMeta","level","permissionMode",
                        "imagePasteIds","attachment"})
    if(message.contains(key))out[key]=message.at(key);
  return out;
}
Json resultJson(const user_input::InputResult& result,const Json& trace){
  Json messages=Json::array();
  for(const auto& message:result.messages)messages.push_back(project(message));
  Json out={{"messages",messages},{"shouldQuery",result.shouldQuery},{"trace",trace}};
  if(result.allowedGadgets)out["allowedTools"]=*result.allowedGadgets;
  if(result.model)out["model"]=*result.model;
  if(result.effort)out["effort"]=*result.effort;
  if(result.resultText)out["resultText"]=*result.resultText;
  return out;
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line)){
    const auto x=value::parse(line);
    event_loop::Loop loop;
    messages::Services messageServices;
    messageServices.randomUUID=[]{return "id";};
    messageServices.nowISO=[]{return "now";};
    std::vector<user_input::CommandEntry> entries;
    const auto& command=value::property(x,"command");
    if(command.is_object()){user_input::CommandEntry entry;entry.descriptor=command;
      entries.push_back(std::move(entry));}
    user_input::SlashServices slash{loop,messageServices,entries};
    user_input::UserInputServices services{loop,messageServices,slash};
    Json trace=Json::array();
    services.getAppState=[](){return Json{{"toolPermissionContext",Json{{"mode","default"}}}};};
    services.setUserInputOnProcessing=[&](const std::string& value){
      trace.push_back("display:"+value);};
    services.queryCheckpoint=[&](const std::string& value){trace.push_back(value);};
    services.feature=[](const std::string&){return false;};
    services.isBridgeSafeCommand=[x](const Json&){return value::property(x,"bridgeSafe")==true;};
    services.resizeImageBlock=[&loop](const Json& block){
      event_loop::Deferred<Json> result(loop);
      auto copy=block;
      const auto& source=value::property(copy,"source");
      const auto& mobile=value::property(source,"mediaType");
      if(mobile.is_string())copy["source"]["media_type"]=mobile;
      result.resolve(Json{{"block",copy},{"dimensions",Json{{"width",10},{"height",20}}}});
      return result;
    };
    services.storeImages=[&loop](const Json& pasted){
      event_loop::Deferred<Json> result(loop);
      Json paths=Json::object();
      for(const auto& key:value::objectKeys(pasted)){
        const auto& id=value::property(pasted.at(key),"id");
        const auto str=id.is_string()?id.get<std::string>():value::stringify(id);
        paths[str]="/saved/"+str;
      }
      result.resolve(std::move(paths));return result;
    };
    services.createImageMetadataText=[](const Json& dims,
      const std::optional<std::string>& path)->std::optional<std::string>{
      return "[Image "+value::numberToString(value::numberOr(value::property(dims,"width")))+
        "x"+value::numberToString(value::numberOr(value::property(dims,"height")))+
        (path?" "+*path:"")+"]";
    };
    services.getAttachmentMessages=[&loop](const std::string& poetry,
      const Json&,const Json&,const Json&,const std::optional<std::string>&){
      event_loop::Deferred<Json> result(loop);
      Json out=Json::array();
      if(poetry.find("attach")!=std::string::npos)
        out.push_back(Json{{"type","attachment"},{"attachment",Json{{"type","file"}}}});
      result.resolve(std::move(out));return result;
    };
    services.executeUserPoetrySubmitHooks=[&loop,x](const std::string&,
      const std::string&,std::stop_token){
      using namespace instructions::hooks::runtime;
      auto items=std::make_shared<Json>(x.value("hooks",Json::array()));
      return HookEventGenerator(loop,[]{return std::optional<ExecuteHooksInput>(
        ExecuteHooksInput{});},[&loop,items](ExecuteHooksInput){
        HookYieldCursor cursor;
        auto index=std::make_shared<std::size_t>(0);
        cursor.next=[&loop,items,index]{
          event_loop::Deferred<HookYield> step(loop);
          if(*index<items->size())step.resolve(HookYield{false,items->at((*index)++),{}});
          else step.resolve(HookYield{true,Json(),{}});
          return step;
        };
        return cursor;
      });
    };
    user_input::UserInputRequest request;
    request.input=x.at("input");
    request.mode=x.value("mode",std::string("prompt"));
    request.pastedContents=value::property(x,"pastedContents");
    request.skipSlashCommands=value::property(x,"skipSlashCommands")==true;
    request.bridgeOrigin=value::property(x,"bridgeOrigin")==true;
    request.isMeta=value::property(x,"isMeta")==true;
    request.skipAttachments=value::property(x,"skipAttachments")==true;
    auto pending=user_input::processUserInput(request,services);
    std::optional<Json> answer;
    pending.then([&](const user_input::InputResult* value,
      const std::exception_ptr& error){
      if(error){try{std::rethrow_exception(error);}catch(const std::exception& e){
        answer=Json{{"error",e.what()},{"trace",trace}};}}
      else answer=resultJson(*value,trace);
    });
    for(int i=0;i<50&&!answer;++i)loop.runReady();
    if(!answer)answer=Json{{"error","unsettled"},{"trace",trace}};
    std::cout<<value::stringify(*answer)<<'\n';
  }
}
