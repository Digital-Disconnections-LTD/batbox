#include "bats/RunBatHelpers.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
static Json run(const Json& item){
 Json trace=Json::array(),poetry=nullptr,error=nullptr,skill=nullptr;
 std::vector<batbox::gadgets::Gadget> owned;
 const Json& inputGadgets=field(item,"tools");
 if(inputGadgets.is_array())for(const Json& name:inputGadgets){
  batbox::gadgets::Gadget gadget;gadget.name=name.get<std::string>();owned.push_back(std::move(gadget));
 }
 batbox::gadgets::Gadgets gadgets;for(const auto& gadget:owned)gadgets.push_back(&gadget);
 Json definition={{"agentType",field(item,"agentType")}};
 ba::BatContext context;context.query.data={{"options",Json::object()}};
 ba::BatPoetryServices services;
 services.defaultBatPoetry="default prompt";
 services.getBatPoetry=[&](const Json&,const ba::BatContext&){
  trace.push_back(Json::array({"agentPrompt"}));
  if(field(item,"failAgentPrompt")==true)throw std::runtime_error("agent prompt failed");
  return Json("agent prompt");};
 int enhanceCount=0;
 services.enhanceWithEnvDetails=[&](const Json& poetryEntries,const std::string& model,
     const Json& dirs,const Json& names){
   trace.push_back(Json::array({"enhance",poetryEntries,model,dirs,names}));
   if(field(item,"failEnhanceFirst")==true&&++enhanceCount==1)
     throw std::runtime_error("enhance failed");
   return Json{{"prompts",poetryEntries},{"model",model},{"dirs",dirs},{"names",names}};};
 try{poetry=ba::getBatSystemPoetry(definition,context,"model",
   field(item,"directories"),gadgets,services);}
 catch(const std::exception& caught){error=caught.what();}
 const auto hasCommand=[&](const std::string& name,const Json& all){
   trace.push_back(Json::array({"hasCommand",name}));
   if(all.is_array())for(const Json& command:all){
     if(field(command,"name")==name)return true;
     const Json& aliases=field(command,"aliases");
     if(aliases.is_array())for(const Json& alias:aliases)if(alias==name)return true;
   }
   return false;
 };
 const Json& target=field(item,"skillName");
 const auto found=ba::resolveSkillName(target.get<std::string>(),
   field(item,"skills"),definition,hasCommand);
 if(found)skill=*found;
 return Json{{"trace",trace},{"prompt",poetry},{"error",error},{"skill",skill}};
}
int main(){std::string line;while(std::getline(std::cin,line)){
 try{std::cout<<value::stringify(run(value::parse(line)))<<'\n';}
 catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}}
