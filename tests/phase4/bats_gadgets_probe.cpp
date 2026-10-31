#include "bats/BatGadgets.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>
using batbox::Json;
namespace ba=batbox::bats;
namespace value=batbox::value;
static const Json& field(const Json& object,const char* key){return value::property(object,key);}
static Json names(const batbox::gadgets::Gadgets& gadgets){
 Json out=Json::array();
 for(const auto* gadget:gadgets)out.push_back(gadget->name);
 return out;
}
static Json run(const Json& item){
 std::vector<batbox::gadgets::Gadget> storage;
 const Json& available=field(item,"available");
 if(available.is_array())for(const Json& name:available){
  batbox::gadgets::Gadget gadget;gadget.name=name.get<std::string>();storage.push_back(std::move(gadget));}
 batbox::gadgets::Gadgets pointers;for(auto& gadget:storage)pointers.push_back(&gadget);
 ba::BatGadgetFilterImports imports;
 imports.allBatDisallowed={"Agent","Hidden"};
 imports.customBatDisallowed={"Write"};
 imports.asyncBatAllowed={"Read","TaskOutput"};
 imports.inProcessPartnerAllowed={"TaskOutput"};
 imports.exitPlanModeGadgetName="ExitPlanMode";imports.batGadgetName="Agent";
 imports.gadgetMatchesName=[](const batbox::gadgets::Gadget& gadget,const std::string& name){
  return gadget.name==name;};
 imports.isBatSwarmsEnabled=[&]{return field(item,"swarms")==true;};
 imports.isInProcessPartner=[&]{return field(item,"teammate")==true;};
 imports.permissionRuleValueFromString=[](const std::string& spec){
  const auto open=spec.find('(');
  if(open!=std::string::npos&&!spec.empty()&&spec.back()==')')
    return Json{{"toolName",spec.substr(0,open)},
      {"ruleContent",spec.substr(open+1,spec.size()-open-2)}};
  return Json{{"toolName",spec}};};
 const Json& definition=field(item,"definition");
 const Json& mode=field(definition,"permissionMode");
 const std::optional<std::string> permission=mode.is_string()?
  std::optional<std::string>(mode.get<std::string>()):std::nullopt;
 const auto filtered=ba::filterGadgetsForBat(pointers,field(definition,"source")=="built-in",
  field(item,"isAsync")==true,permission,imports);
 const auto result=ba::resolveBatGadgets(definition,pointers,
  field(item,"isAsync")==true,field(item,"isMainThread")==true,imports);
 return Json{{"filtered",names(filtered)},{"hasWildcard",result.hasWildcard},
  {"validTools",result.validGadgets},{"invalidTools",result.invalidGadgets},
  {"resolvedTools",names(result.resolvedGadgets)},
  {"allowedAgentTypes",result.allowedBatTypes?
    Json(*result.allowedBatTypes):Json(nullptr)}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{
 std::cout<<value::stringify(run(value::parse(line)))<<'\n';
 }catch(const std::exception& exception){std::cerr<<exception.what()<<'\n';return 1;}}
