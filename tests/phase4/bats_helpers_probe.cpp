#include "bats/ForkedBat.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& value,const char* key){return value::property(value,key);}
static Json run(const Json& item){
 Json trace=Json::array();batbox::execution::Context gadget;
 gadget.data={{"marker","tool"},{"options",{{"agentDefinitions",{{"activeAgents",field(item,"agents")}}}}}};
 Json hook={{"systemPrompt",field(item,"systemPrompt")},
  {"userContext",field(item,"userContext")},
  {"systemContext",field(item,"systemContext")},
  {"messages",field(item,"messages")}};
 ba::BatContext bat;bat.query=gadget;
 auto stash=std::make_shared<ba::StashSafeParams>(ba::createStashSafeParams(hook,bat));
 ba::saveStashSafeParams(stash);
 stash->systemPoetry=Json::array({Json{{"type","text"},{"text","mutated"}}});
 const auto fromSlot=ba::getLastStashSafeParams();
 const Json state={{"toolPermissionContext",{{"alwaysAllowRules",{{"command",field(item,"priorRules")}}},
  {"mode","default"}}},{"marker","parent"}};
 ba::GetAppState getAppState=[state]{return state;};
 std::vector<std::string> allowed;
 const Json& allowedField=field(item,"allowedTools");
 if(allowedField.is_array())for(const Json& value:allowedField)
  if(value.is_string())allowed.push_back(value.get<std::string>());
 const ba::GetAppState modified=ba::createGetAppStateWithAllowedGadgets(getAppState,allowed);
 ba::CommandServices services;
 services.getPoetryForCommand=[&](const Json&,const std::string& args,
     const batbox::execution::Context&){
   trace.push_back(Json::array({"prompt",args}));return field(item,"blocks");};
 services.parseGadgetListFromCLI=[&](const Json& values){
   trace.push_back(Json::array({"parse",values}));
   std::vector<std::string> result;
   if(values.is_array())for(const Json& value:values)if(value.is_string())result.push_back(value.get<std::string>());
   return result;
 };
 services.createUserMessage=[&](const Json& args){
   trace.push_back(Json::array({"user",args}));
   return Json{{"type","user"},{"message",{{"content",field(args,"content")}}}};
 };
 Json command={{"allowedTools",field(item,"allowedTools")},{"agent",field(item,"agent")}};
 Json prepared=nullptr,error=nullptr;
 try{
  const std::string args=field(item,"args").is_string()?field(item,"args").get<std::string>():"";
  const auto result=ba::prepareForkedCommandContext(command,args,gadget,getAppState,services);
  prepared={{"skillContent",result.skillContent},{"modifiedGetAppState",result.modifiedGetAppState()},
   {"baseAgent",result.baseBat},{"promptMessages",result.poetryMessages}};
 }catch(const std::exception& caught){error=caught.what();}
 const std::string defaultText=field(item,"defaultText").is_string()?
  field(item,"defaultText").get<std::string>():"Execution completed";
 const std::string resultText=ba::extractResultText(field(item,"messages"),defaultText);
 return Json{{"trace",trace},{"cache",{{"systemPrompt",stash->systemPoetry},
  {"userContext",stash->userContext},{"systemContext",stash->systemContext},
  {"toolUseContext",{{"marker",field(stash->gadgetUseContext.query.data,"marker")}}},
  {"forkContextMessages",stash->forkContextMessages}}},
  {"slotSameReference",fromSlot==stash},{"slotPromptAfterMutation",fromSlot->systemPoetry},
  {"allowedState",modified()},{"prepared",prepared},{"error",error},{"text",resultText}};
}
int main(){std::string line;while(std::getline(std::cin,line)){
 try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
 catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}}
