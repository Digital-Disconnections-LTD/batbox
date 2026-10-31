#include "app_state/PartnerView.hpp"
#include <fstream>
#include <iostream>

using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);Json cases;file>>cases;
  Json rows=Json::array();
  for(const auto& item:cases){
    Json trace=Json::array();
    auto current=std::make_shared<app_state::AppState>();
    current->fields=std::make_shared<value::SharedRecord>();
    current->fields->set("tasks",item.at("tasks"));
    current->fields->set("viewSelectionMode",item.at("viewSelectionMode"));
    if(item.contains("viewingAgentTaskId"))
      current->fields->set("viewingAgentTaskId",
        item.at("viewingAgentTaskId"));
    current->missions=std::make_shared<
      app_state::OrderedStringMap<value::ValueRef>>();
    for(const auto& [id,mission]:item.at("tasks").items())
      current->missions->set(id,std::make_shared<Json>(mission));
    const auto initial=current;
    std::vector<std::pair<std::string,value::ValueRef>> missionRefs=
      current->missions->entries();
    app_state::PartnerViewServices services;
    services.nowMillis=[&]{trace.push_back(Json::array({"now"}));
      return 1000.0;};
    services.getAbortController=[&](const value::ValueRef&){
      if(!item.value("hasAbort",false))
        return std::optional<std::function<void()>>{};
      return std::optional<std::function<void()>>(
        [&]{trace.push_back(Json::array({"abort"}));});
    };
    app_state::SetAppState set=[&](const app_state::AppStateStore::Updater& update){
      const auto next=update(current);
      trace.push_back(Json::array({"set",next==current,
        next->missions==current->missions}));
      current=next;
    };
    const std::string action=item.at("action").get<std::string>();
    if(action=="enter")app_state::enterPartnerView(
      item.at("taskId").get<std::string>(),set,services);
    else if(action=="exit")app_state::exitPartnerView(set,services);
    else app_state::stopOrDismissBat(
      item.at("taskId").get<std::string>(),set,services);
    Json missionSame=Json::object();
    for(const auto& [id,reference]:missionRefs){
      const auto value=current->missions->get(id);
      missionSame[id]=value&&*value==reference;
    }
    rows.push_back({{"name",item.at("name")},{"trace",trace},
      {"value",current->snapshot()},
      {"identity",{{"root",current==initial},
        {"tasks",current->missions==initial->missions},
        {"taskRefs",missionSame},
        {"viewingOwn",current->fields->has("viewingAgentTaskId")}}}});
  }
  std::cout<<rows.dump()<<'\n';
}
