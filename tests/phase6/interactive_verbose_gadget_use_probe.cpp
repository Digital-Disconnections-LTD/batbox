#include "interactive/cards_collapsed/VerboseGadgetUse.hpp"
#include <iostream>

using namespace batbox;
int main(){
  Json cases;std::cin>>cases;
  Json output=Json::array();
  for(const auto& item:cases){
    std::vector<std::string> trace;
    auto add=[&](std::string value){trace.push_back(std::move(value));};
    gadgets::Gadget gadget;gadget.name="Bash";gadget.outputSchema=Json::object();
    if(item.value("tag",false))gadget.renderGadgetUseTag=[](const Json&){return Json();};
    if(item.value("result",false))gadget.renderGadgetResultMessage=[](
      const Json&,const Json&,const Json&){return Json();};
    gadgets::Gadgets available;
    if(item.value("hasTool",false))available.push_back(&gadget);
    interactive::MessageLookups lookups;
    if(item.value("resolved",false))lookups.resolvedGadgetUseIDs.insert("id");
    if(item.value("error",false))lookups.erroredGadgetUseIDs.insert("id");
    if(item.value("hasOutput",false))
      lookups.gadgetResultByGadgetUseID["id"]=std::make_shared<Json>(Json{
        {"type","user"},{"toolUseResult",item.value("nullOutput",false)?
          Json():Json{{"ok",true}}}});
    interactive::GadgetIDs active;
    if(item.value("active",false))active.insert("id");
    interactive::cards_collapsed::VerboseGadgetUseServices services;
    services.readSelectedBackground=[&]{add("bg");return Json();};
    services.primitiveGadgets=[&]{add("primitive");return item.value("primitive",false)?
      gadgets::Gadgets{&gadget}:gadgets::Gadgets{};};
    services.safeParseOutput=[&](const gadgets::Gadget&,
      const std::optional<Json>& raw)->std::optional<Json>{
      add("parseOutput");return item.value("outputValid",false)?raw:std::nullopt;};
    services.safeParseInput=[&](const gadgets::Gadget&,const Json& raw)->std::optional<Json>{
      add("parseInput");return item.value("valid",false)?std::optional<Json>(raw):
        std::nullopt;};
    services.userFacingName=[&](const gadgets::Gadget&,const std::optional<Json>&){
      add("name");return std::string("Bash");};
    services.renderUse=[&](const gadgets::Gadget&,const Json&,const Json&){
      add("renderUse");return Json("USE");};
    services.renderTag=[&](const gadgets::Gadget&,const Json&){add("tag");return Json("TAG");};
    services.renderResult=[&](const gadgets::Gadget&,const Json&,
      const gadgets::Gadgets&,const Json&){add("result");return Json("RESULT");};
    const auto card=interactive::cards_collapsed::classifyVerboseGadgetUse(
      Json{{"id","id"},{"name","Bash"},{"input",{{"command","pwd"}}}},
      available,lookups,active,true,Json("theme"),services);
    output.push_back(Json{{"case",item["name"]},{"visible",card.visible},
      {"name",card.visible&&!card.name.empty()},
      {"use",card.visible&&card.useMessage=="USE"},
      {"tag",card.visible&&card.tag=="TAG"},
      {"result",card.visible&&card.resultMessage=="RESULT"},
      {"trace",trace}});
  }
  std::cout<<output.dump()<<'\n';
}
