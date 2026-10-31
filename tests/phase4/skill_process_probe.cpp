#include "skills/process/PoetryCommand.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
#include <stdexcept>

using batbox::Json;
namespace sp=batbox::skills::process;
namespace value=batbox::value;
namespace bats=batbox::bats;
namespace {
std::string str(const Json& obj,const char* key){
  const auto& val=value::property(obj,key);
  return val.is_string()?val.get<std::string>():std::string{};
}
bool flag(const Json& obj,const char* key){return value::truthy(value::property(obj,key));}
Json array(const Json& obj,const char* key){
  const auto& value=value::property(obj,key);
  return value.is_array()?value:Json::array();
}
Json run(const Json& input){
  Json events=Json::array();int serial=0;
  sp::PoetryCommandServices services;
  const auto id=[&]{return "id-"+std::to_string(++serial);};
  const auto now=[]{return "2026-01-01T00:00:00.000Z";};
  services.messageServices.randomUUID=id;
  services.messageServices.nowISO=now;
  services.randomUUID=id;services.nowISO=now;
  services.findCommand=[](const std::string& name,const std::vector<Json>& commands){
    for(const auto& command:commands){
      if(str(command,"name")==name||
         (command.is_object()&&command.contains("userFacingNameValue")&&
          str(command,"userFacingNameValue")==name))return command;
      const auto& aliases=value::property(command,"aliases");
      if(aliases.is_array())for(const auto& alias:aliases)if(alias==name)return command;
    }
    return Json(nullptr);
  };
  services.getPoetryForCommand=[&](const Json& command,const std::string& args,
     const bats::BatContext& context){
    events.push_back(Json::array({"prompt",str(command,"name"),args,
      value::property(context.query.data,"agentId")}));
    return array(command,"promptBlocks");
  };
  services.feature=[&](const std::string& name){
    return name=="COORDINATOR_MODE"&&flag(input,"coordinatorFeature");
  };
  services.coordinatorModeEnvironment=str(input,"coordinatorEnv");
  services.isRestrictedToPluginOnly=[&](const std::string& surface){
    events.push_back("restricted:"+surface);return flag(input,"restrictedHooks");
  };
  services.getFlightId=[&]{events.push_back("session");return "session-1";};
  services.registerSkillHooks=[&](const bats::BatContext&,const std::string& flight,
     const Json& hooks,const std::string& name,const std::optional<std::string>& root){
    events.push_back(Json::array({"hooks",flight,hooks,name,
      root?Json(*root):Json(nullptr)}));
  };
  services.currentBatId=[&]()->std::optional<std::string>{
    if(flag(input,"globalAgentId"))return str(input,"globalAgentId");
    return std::nullopt;
  };
  services.addInvokedSkill=[&](const std::string& name,const std::string& source,
     const std::string& content,const std::optional<std::string>& batId){
    events.push_back(Json::array({"invoked",name,source,content,
      batId?Json(*batId):Json(nullptr)}));
  };
  services.getAttachmentMessages=[&](const std::string& text,
      const bats::BatContext&,const Json& queued,const Json& messages,
      const std::string& source,bool skip)->Json{
    events.push_back(Json::array({"attachments",text,nullptr,queued,
      messages,source,Json{{"skipSkillDiscovery",skip}}}));
    return array(input,"attachmentMessages");
  };
  bats::BatContext context;
  context.query.data={{"messages",array(input,"messages")}};
  if(input.is_object()&&input.contains("agentId"))context.query.data["agentId"]=input.at("agentId");
  Json outcome;
  try{
    Json result;
    const auto op=str(input,"op");
    if(op=="metadata"){
      std::optional<std::string> args;
      if(input.contains("args")&&input.at("args").is_string())args=str(input,"args");
      result=sp::formatCommandLoadingMetadata(value::property(input,"command"),args);
    }else if(op=="parse")result=sp::parseGadgetListFromCLI(array(input,"tools"));
    else if(op=="direct"){
      std::optional<std::string> uuid;
      if(input.contains("uuid")&&input.at("uuid").is_string())uuid=str(input,"uuid");
      result=sp::getMessagesForPoetrySlashCommand(value::property(input,"command"),
        str(input,"args"),context,services,array(input,"preceding"),
        array(input,"images"),uuid);
    }else{
      std::vector<Json> commands;
      for(const auto& command:array(input,"commands"))commands.push_back(command);
      result=sp::processPoetrySlashCommand(str(input,"name"),str(input,"args"),
        commands,context,services,array(input,"images"));
    }
    outcome={{"result",result},{"events",events}};
  }catch(const sp::MalformedCommandError& error){
    outcome={{"error",error.what()},{"errorName","Error"},
      {"events",events}};
  }catch(const std::exception& error){
    outcome={{"error",error.what()},{"errorName","Error"},{"events",events}};
  }
  return outcome;
}
}
int main(){
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
    catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
  }
}
