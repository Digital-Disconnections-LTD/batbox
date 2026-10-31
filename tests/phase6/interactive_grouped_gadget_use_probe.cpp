#include "interactive/cards_gadget_use/GroupedGadgetUse.hpp"
#include <iostream>

using namespace batbox;
int main(){
  Json cases;std::cin>>cases;
  Json output=Json::array();
  for(const auto& item:cases){
    gadgets::Gadget gadget;gadget.name="Agent";
    if(item.value("render",false))
      gadget.renderGroupedGadgetUse=[](const Json&,const Json&){return Json();};
    gadgets::Gadgets available;
    if(item.value("toolPresent",false))available.push_back(&gadget);
    Json message={{"toolName","Agent"},{"messages",Json::array()},
                  {"results",Json::array()}};
    for(const char* id:{"a","b"})
      message["messages"].push_back(Json{{"message",Json{{"content",
        Json::array({Json{{"type","tool_use"},{"id",id}}})}}}});
    for(const auto& result:item["results"]){
      Json wrapped={{"message",Json{{"content",
        Json::array({Json{{"type","tool_result"},
                          {"tool_use_id",result["id"]}}})}}}};
      if(result.contains("output"))wrapped["toolUseResult"]=result["output"];
      message["results"].push_back(std::move(wrapped));
    }
    interactive::MessageLookups lookups;
    lookups.resolvedGadgetUseIDs.insert("a");
    lookups.erroredGadgetUseIDs.insert("b");
    for(const auto& progress:item["progress"])
      lookups.progressMessagesByGadgetUseID["a"].push_back(
        std::make_shared<Json>(progress));
    interactive::GadgetIDs active;
    for(const auto& id:item["inProgress"])active.insert(id.get<std::string>());
    const auto view=interactive::cards_gadget_use::renderGroupedGadgetUse(
      message,available,lookups,active,item.value("animate",false),
      [&](const gadgets::Gadget&,
         const std::vector<interactive::cards_gadget_use::GroupedGadgetUseEntry>& entries,
         bool animate,const gadgets::Gadgets& all){
        if(item["name"]=="undefined-output"){
          if(!entries.at(0).result||entries.at(0).result->output)
            throw std::logic_error("Undefined grouped output was not preserved");
        }
        Json data=Json::array();
        for(const auto& entry:entries){
          Json value={{"param",entry.param},{"isResolved",entry.isResolved},
            {"isError",entry.isError},{"isInProgress",entry.isInProgress},
            {"progressMessages",entry.progressMessages}};
          if(entry.result){
            Json result={{"param",entry.result->param}};
            if(entry.result->output)result["output"]=*entry.result->output;
            value["result"]=std::move(result);
          }
          data.push_back(std::move(value));
        }
        return Json{{"data",data},{"animate",animate},
          {"toolsCount",all.size()}};
      });
    output.push_back(Json{{"name",item["name"]},{"view",view}});
  }
  std::cout<<output.dump()<<'\n';
}
