#include "interactive/presentation_providers/MissionFooter.hpp"
#include <ftxui/dom/elements.hpp>
#include <algorithm>

namespace batbox::interactive::presentation_providers {
namespace {
std::string text(const Json& object,const char* key){
  const auto& value=value::property(object,key);
  return value.is_string()?value.get<std::string>():"";
}
std::string clipped(std::string value,int columns){
  const auto newline=value.find_first_of("\r\n");
  if(newline!=std::string::npos)value.resize(newline);
  const auto width=static_cast<std::size_t>(std::max(3,columns));
  return value::stringLength(value)>width?value::slice(value,0,width-1)+"…":value;
}
}
Json backgroundFooterMissions(const Json& missions){
  Json active=Json::array();
  if(missions.is_object())for(auto it=missions.begin();it!=missions.end();++it){
    const auto& mission=it.value();const auto status=text(mission,"status");
    if((status!="running"&&status!="pending")||
        value::property(mission,"isBackgrounded")==false)continue;
    auto row=mission;row["id"]=it.key();active.push_back(std::move(row));
  }
  return active;
}
std::size_t visibleFooterMissions(const Json& missions,int rows){
  return std::min(missions.size(),static_cast<std::size_t>(
    std::clamp(rows/4-1,1,4)));
}
ftxui::Element missionFooter(const Json& missions,int columns,int rows,
    ftxui::Color color){
  ftxui::Elements lines;
  if(missions.empty())return ftxui::vbox(std::move(lines));
  lines.push_back(ftxui::text(clipped("Background Missions ("+
    std::to_string(missions.size())+") · /Missions",columns))|ftxui::color(color)|ftxui::dim);
  for(std::size_t i=0;i<visibleFooterMissions(missions,rows);++i){
    const auto& mission=missions[i];
    auto label=text(mission,"description");
    if(label.empty())label=text(mission,"agentType");
    if(label.empty())label=text(mission,"type");
    std::string detail=text(mission,"status");
    const auto& progress=value::property(mission,"progress");
    const auto& count=value::property(progress,"toolUseCount");
    const auto& fallback=value::property(mission,"toolUseCount");
    const auto& gadgets=count.is_number()?count:fallback;
    if(gadgets.is_number())detail+=" · "+std::to_string(gadgets.get<int>())+" Gadgets";
    const auto& activity=value::property(progress,"lastActivity");
    auto last=text(activity,"activityDescription");
    if(last.empty())last=text(activity,"toolName");
    if(!last.empty())detail+=" · "+last;
    lines.push_back(ftxui::hbox({ftxui::text("  ● ")|ftxui::color(color),
      ftxui::text(clipped(label+" · "+detail,columns-4))}));
  }
  return ftxui::vbox(std::move(lines));
}
}
