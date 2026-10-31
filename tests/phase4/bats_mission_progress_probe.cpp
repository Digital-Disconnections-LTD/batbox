#include "bats/BatMissionProgress.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
static Json run(const Json& item){
 std::vector<batbox::gadgets::Gadget> storage;
 const Json& names=field(item,"tools");
 if(names.is_array())for(const Json& name:names){
  batbox::gadgets::Gadget gadget;gadget.name=name.get<std::string>();
  gadget.getActivityDescription=[label=gadget.name](const Json& input){
   const Json& path=field(input,"path");
   return Json("activity:"+label+":"+
    (path.is_string()?path.get<std::string>():""));};
  storage.push_back(std::move(gadget));}
 batbox::gadgets::Gadgets gadgets;for(auto& gadget:storage)gadgets.push_back(&gadget);
 ba::BatMissionProgressImports imports;
 imports.syntheticOutputGadgetName="StructuredOutput";
 imports.getGadgetSearchOrReadInfo=[](const std::string& name,const Json&,
   const batbox::gadgets::Gadgets&){return Json{{"isSearch",name=="Grep"},
    {"isRead",name=="Read"}};};
 Json tracker=ba::createProgressTracker(),updates=Json::array();
 const auto resolver=ba::createActivityDescriptionResolver(gadgets);
 const Json& messages=field(item,"messages");
 if(messages.is_array())for(const Json& message:messages){
  ba::updateProgressFromMessage(tracker,message,resolver,gadgets,imports);
  updates.push_back(ba::getProgressUpdate(tracker));}
 return Json{{"tracker",tracker},{"updates",updates},
  {"tokenCount",ba::getTokenCountFromTracker(tracker)}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{
 std::cout<<value::stringify(run(value::parse(line)))<<'\n';
 }catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}}
