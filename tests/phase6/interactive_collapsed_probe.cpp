#include "interactive/cards_collapsed/CollapsedPlan.hpp"
#include "runtime/JsonValue.hpp"
#include <iostream>

using namespace batbox;
int main(){
  Json cases;std::cin>>cases;
  Json output=Json::array();
  interactive::cards_collapsed::CollapsedState state;
  for(const auto& item:cases){
    if(item.value("reset",false))state={};
    Json message={{"searchCount",0},{"readCount",0},{"listCount",0},
      {"replCount",0},{"memorySearchCount",0},{"memoryReadCount",0},
      {"memoryWriteCount",0},{"messages",Json::array({Json{
        {"type","assistant"},{"message",Json{{"content",Json::array({Json{
          {"type","tool_use"},{"id","id"}}})}}}}})}};
    message.merge_patch(item["message"]);
    interactive::MessageLookups lookups;
    if(item.contains("progress"))
      lookups.progressMessagesByGadgetUseID["id"].push_back(
        std::make_shared<Json>(Json{{"data",item["progress"]}}));
    interactive::GadgetIDs activeIDs;
    if(item.value("activeID",false))activeIDs.insert("id");
    gadgets::Gadgets available;
    interactive::cards_collapsed::CollapsedServices services;
    services.readSelectedBackground=[](){return Json();};
    services.readTheme=[](){return Json("theme");};
    services.isFullscreen=[&](){return item.value("fullscreen",false);};
    services.teamMemoryEnabled=[&](){return item.value("team",false);};
    services.displayPath=[](const std::string& path){return "display:"+path;};
    services.minimumDisplayTime=[](const std::optional<std::string>& value,int){
      return value;};
    services.formatDuration=[](double value){return "D"+value::numberToString(value);};
    services.formatSecondsShort=[](double value){return "S"+value::numberToString(value);};
    services.expandHint=[](){return std::string{};};
    services.stripMcpServerPrefix=[](const std::string& value){
      const std::string prefix="claude.ai ";
      return value.starts_with(prefix)?value.substr(prefix.size()):value;};
    interactive::cards_collapsed::CollapsedInputs input;
    input.message=std::move(message);input.inProgressGadgetUseIDs=&activeIDs;
    input.active=item.value("active",false);input.lookups=&lookups;input.gadgets=&available;
    const auto card=interactive::cards_collapsed::classifyCollapsedReadSearch(
      state,input,services);
    std::string text;
    if(card.visible){
      for(const auto& span:card.summary)text+=span.text;
      if(card.active)text+="…";
      text+=' ';
      if(card.active&&!card.hintLines.empty()){
        for(const auto& line:card.hintLines)text+="  ⎿  "+line;
      }
      if(card.hookSummary)text+="  ⎿  "+*card.hookSummary;
    }
    output.push_back(Json{{"name",item["name"]},{"visible",card.visible},
                          {"text",text}});
  }
  std::cout<<output.dump()<<'\n';
}
