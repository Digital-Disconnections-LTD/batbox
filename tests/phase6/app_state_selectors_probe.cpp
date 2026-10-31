#include "app_state/Selectors.hpp"
#include <fstream>
#include <iostream>

using namespace batbox;
int main(int argc,char** argv){
  if(argc!=2)return 2;
  std::ifstream file(argv[1]);Json cases;file>>cases;
  Json rows=Json::array();
  for(const auto& item:cases){
    app_state::AppState state;
    state.fields=std::make_shared<value::SharedRecord>();
    state.missions=std::make_shared<app_state::OrderedStringMap<value::ValueRef>>();
    if(item.contains("viewingAgentTaskId"))
      state.fields->set("viewingAgentTaskId",item.at("viewingAgentTaskId"));
    for(const auto& [key,value]:item.at("tasks").items())
      state.missions->set(key,std::make_shared<Json>(value));
    const auto viewed=app_state::getViewedPartnerMission(state);
    const auto active=app_state::getActiveBatForInput(state);
    Json same=nullptr;
    const auto id=value::property(item,"viewingAgentTaskId");
    if(id.is_string()&&(viewed||active.mission)){
      const auto mission=state.missions->get(id.get<std::string>());
      same=mission&&*mission&&(viewed?viewed==*mission:active.mission==*mission);
    }
    rows.push_back({{"name",item.at("name")},
      {"viewed",viewed?*viewed:Json(nullptr)},
      {"active",active.snapshot()},{"sameTask",same}});
  }
  std::cout<<rows.dump()<<'\n';
}
