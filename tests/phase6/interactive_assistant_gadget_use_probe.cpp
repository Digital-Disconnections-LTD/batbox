#include "interactive/cards_gadget_use/GadgetUseDecision.hpp"
#include <iostream>

using namespace batbox;
int main(){
  Json cases;std::cin>>cases;
  Json output=Json::array();
  for(const auto& item:cases){
    std::vector<std::string> trace;
    auto add=[&](std::string event){trace.push_back(std::move(event));};
    gadgets::Gadget gadget;gadget.name="Bash";
    gadget.userFacingNameBackgroundColor=[](const Json&){return Json();};
    gadget.isTransparentWrapper=[&]{add("transparent");return item.value("transparent",false);};
    gadget.renderGadgetUseMessage=[](const Json&,const Json&){return Json();};
    if(item.value("tag",false))gadget.renderGadgetUseTag=[](const Json&){return Json();};
    if(item.value("progress",false))
      gadget.renderGadgetUseProgressMessage=[](const Json&,const Json&){return Json();};
    if(item.value("queued",false))gadget.renderGadgetUseQueuedMessage=[](){return Json();};
    gadgets::Gadgets available;
    if(item.value("hasTool",false))available.push_back(&gadget);
    interactive::MessageLookups lookups;
    if(item.value("resolved",false))lookups.resolvedGadgetUseIDs.insert("id");
    interactive::GadgetIDs active;
    if(item.value("active",false))active.insert("id");
    interactive::cards_gadget_use::UseServices services;
    services.readTerminalSize=[&]{add("terminal");return interactive::cards_gadget_use::TerminalSize{80,24};};
    services.readTheme=[&]{add("theme");return Json("theme");};
    services.readSelectedBackground=[&]{add("bg");return Json();};
    services.readPendingWorkerGadgetID=[&]() -> std::optional<std::string> {
      add("app");return item.value("waiting",false)?std::optional<std::string>("id"):std::nullopt;};
    services.readClassifierChecking=[&](const std::string&){add("classifier");return false;};
    services.readPermissionMode=[&]{add("app");return std::string("default");};
    services.readStrippedDangerousRules=[&]{add("app");return false;};
    services.logError=[&](const std::string& text){add("log:"+text);};
    services.safeParseInput=[&](const gadgets::Gadget&,const Json& value)->std::optional<Json>{
      add("parse");return item.value("valid",false)?std::optional<Json>(value):std::nullopt;};
    services.userFacingName=[&](const gadgets::Gadget&,const std::optional<Json>&){
      add("name");return item.value("displayName",std::string{});};
    services.userFacingNameBackgroundColor=[&](const gadgets::Gadget&,const std::optional<Json>&){
      add("nameBg");return Json();};
    services.stringWidth=[&](const std::string& value){add("width");return static_cast<int>(value.size());};
    services.renderGadgetUse=[&](const gadgets::Gadget&,const Json&,
      const interactive::cards_gadget_use::UseRenderContext&){
      add("renderUse");return item.value("renderNull",false)?Json():
        item.value("renderEmpty",false)?Json(""):Json("USE");};
    services.renderTag=[&](const gadgets::Gadget&,const Json&){add("tag");return Json("TAG");};
    services.renderProgress=[&](const gadgets::Gadget&,const Json& progress,
      const interactive::cards_gadget_use::ProgressRenderContext& context){
      add("progress:"+std::to_string(progress.size())+":"+
        std::to_string(context.inProgressGadgetCallCount));return Json("PROGRESS");};
    services.renderQueued=[&](const gadgets::Gadget&){add("queued");return Json("QUEUED");};
    interactive::cards_gadget_use::GadgetUseInputs input;
    input.param={{"id","id"},{"name","Bash"},{"input",{{"command","pwd"}}}};
    input.gadgets=&available;input.inProgressGadgetUseIDs=&active;input.lookups=&lookups;
    input.shouldShowDot=true;input.transcriptMode=item.value("transcript",false);
    input.progressMessages=Json::array({Json{{"data",{{"type","hook_progress"}}}},
      Json{{"data",{{"type","tool_progress"}}}}});
    const auto card=interactive::cards_gadget_use::classifyGadgetUse(input,services);
    output.push_back(Json{{"name",item["name"]},{"visible",card.visible},
      {"transparent",card.visible&&card.transparent},
      {"use",card.visible&&card.useText=="USE"},
      {"tag",card.visible&&card.tag=="TAG"},
      {"waiting",card.visible&&card.progressKind==
        interactive::cards_gadget_use::GadgetUseCard::ProgressKind::WaitingPermission},
      {"queued",card.visible&&card.queuedMessage=="QUEUED"},
      {"progress",card.visible&&card.progress=="PROGRESS"},
      {"hook",card.visible&&card.progressKind==
        interactive::cards_gadget_use::GadgetUseCard::ProgressKind::GadgetProgress},
      {"trace",trace}});
  }
  std::cout<<output.dump()<<'\n';
}
