#include "gadgets/Gadget.hpp"
#include <iostream>
#include <string>

using batbox::Json;namespace gadgets=batbox::gadgets;
static Json evaluate(const Json& t){
  const std::string op=t.at("op").get<std::string>();
  if(op=="progress")return Json{{"result",gadgets::filterGadgetProgressMessages(t.at("messages"))}};
  if(op=="permission")return Json{{"result",gadgets::getEmptyGadgetPermissionContext()}};
  if(op=="matches"){
    gadgets::Gadget gadget;gadget.name=t.at("tool").at("name").get<std::string>();
    if(t.at("tool").contains("aliases"))for(const auto& alias:t.at("tool").at("aliases"))gadget.aliases.push_back(alias.get<std::string>());
    return Json{{"result",gadgets::gadgetMatchesName(gadget,t.at("name").get<std::string>())}};
  }
  if(op=="find"){
    std::vector<gadgets::Gadget> storage;gadgets::Gadgets pointers;
    for(const auto& item:t.at("tools")){
      gadgets::Gadget gadget;gadget.name=item.at("name").get<std::string>();
      if(item.contains("aliases"))for(const auto& alias:item.at("aliases"))gadget.aliases.push_back(alias.get<std::string>());
      storage.push_back(std::move(gadget));
    }
    for(const auto& gadget:storage)pointers.push_back(&gadget);
    const auto* found=gadgets::findGadgetByName(pointers,t.at("name").get<std::string>());
    return Json{{"result",found?Json(found->name):Json()}};
  }
  gadgets::Gadget def;def.name=t.at("name").get<std::string>();
  if(t.contains("enabled"))def.isEnabled=[&](){return t.at("enabled").get<bool>();};
  if(t.contains("concurrencySafe"))def.isConcurrencySafe=[&](const Json&){return t.at("concurrencySafe").get<bool>();};
  if(t.contains("readOnly"))def.isReadOnly=[&](const Json&){return t.at("readOnly").get<bool>();};
  if(t.contains("destructive"))def.isDestructive=[&](const Json&){return t.at("destructive").get<bool>();};
  if(t.contains("classifier"))def.toAutoClassifierInput=[&](const Json&){return t.at("classifier");};
  if(t.contains("userFacingName"))def.userFacingName=[&](const Json&){return t.at("userFacingName").get<std::string>();};
  if(t.contains("permission"))def.checkPermissions=[&](const Json&,const Json&){std::promise<Json> p;p.set_value(t.at("permission"));return p.get_future();};
  const gadgets::Gadget gadget=gadgets::buildGadget(std::move(def));
  const Json input=t.value("input",Json::object());
  return Json{{"result",Json{{"name",gadget.name},{"isEnabled",gadget.isEnabled()},
    {"isConcurrencySafe",gadget.isConcurrencySafe(Json::object())},{"isReadOnly",gadget.isReadOnly(Json::object())},
    {"isDestructive",gadget.isDestructive(Json::object())},
    {"checkPermissions",gadget.checkPermissions(input,Json()).get()},
    {"toAutoClassifierInput",gadget.toAutoClassifierInput(Json::object())},
    {"userFacingName",gadget.userFacingName(Json::object())}}}};
}
int main(){std::string line;while(std::getline(std::cin,line)){
  try{std::cout<<batbox::value::stringify(evaluate(batbox::value::parse(line)))<<'\n';}
  catch(const std::exception& e){std::cout<<batbox::value::stringify(Json{{"error",e.what()}})<<'\n';}
}}
