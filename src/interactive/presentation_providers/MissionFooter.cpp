#include "interactive/presentation_providers/MissionFooter.hpp"
#include "runtime/Platform.hpp"
#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

namespace batbox::interactive::presentation_providers {
namespace {
std::string text(const Json& object,const char* key){
  const auto& value=value::property(object,key);
  return value.is_string()?value.get<std::string>():"";
}
long long number(const Json& object,const char* key){
  const auto& value=value::property(object,key);
  return value.is_number()?static_cast<long long>(value.get<double>()):0;
}
bool present(const Json& object,const char* key){
  return value::property(object,key).is_number();
}
std::string clipped(std::string value,int columns){
  const auto newline=value.find_first_of("\r\n");
  if(newline!=std::string::npos)value.resize(newline);
  const auto width=static_cast<std::size_t>(std::max(3,columns));
  return value::stringLength(value)>width?value::slice(value,0,width-1)+"…":value;
}
bool active(const std::string& status){return status=="running"||status=="pending";}
}
std::string compactDuration(long long milliseconds){
  const auto seconds=std::max(0LL,milliseconds)/1000;
  if(seconds<60)return std::to_string(seconds)+"s";
  const auto minutes=seconds/60;
  if(minutes<60)return std::to_string(minutes)+"m "+std::to_string(seconds%60)+"s";
  const auto hours=minutes/60;
  if(hours<24)return std::to_string(hours)+"h "+std::to_string(minutes%60)+"m";
  return std::to_string(hours/24)+"d "+std::to_string(hours%24)+"h";
}
std::string compactTokens(long long tokens){
  if(tokens<1000)return std::to_string(std::max(0LL,tokens))+" tokens";
  const auto tenths=static_cast<long long>(std::llround(static_cast<double>(tokens)/100.0));
  if(tenths<1000)return std::to_string(tenths/10)+"."+std::to_string(tenths%10)+"k tokens";
  return std::to_string((tokens+500)/1000)+"k tokens";
}
BatGlance batGlance(const Json& mission,long long nowMs){
  BatGlance glance;
  glance.label=text(mission,"name");
  if(glance.label.empty())glance.label=text(mission,"description");
  if(glance.label.empty())glance.label=text(mission,"agentType");
  if(glance.label.empty())glance.label=text(mission,"type");
  glance.status=text(mission,"status");
  glance.running=active(glance.status);
  glance.foreground=glance.running&&value::property(mission,"isBackgrounded")==false;
  glance.model=text(mission,"model");
  const auto& progress=value::property(mission,"progress");
  const auto& result=value::property(mission,"result");
  glance.gadgetUses=present(progress,"toolUseCount")?number(progress,"toolUseCount"):
    present(result,"totalToolUseCount")?number(result,"totalToolUseCount"):
    number(mission,"toolUseCount");
  glance.tokens=present(progress,"tokenCount")?number(progress,"tokenCount"):
    number(result,"totalTokens");
  const auto start=number(mission,"startTime");
  if(start>0){
    const auto end=glance.running?nowMs:
      present(mission,"endTime")?number(mission,"endTime"):
      present(result,"totalDurationMs")?start+number(result,"totalDurationMs"):0;
    if(end>0)glance.elapsed=compactDuration(end-start);
  }
  const auto& activity=value::property(progress,"lastActivity");
  glance.gadget=text(activity,"activityDescription");
  if(glance.gadget.empty())glance.gadget=text(activity,"toolName");
  glance.lastText=text(progress,"lastText");
  return glance;
}
Json backgroundFooterMissions(const Json& missions){
  std::vector<Json> rows;
  if(missions.is_object())for(auto it=missions.begin();it!=missions.end();++it){
    const auto& mission=it.value();
    if(!active(text(mission,"status"))||text(mission,"agentType")=="main-session")continue;
    // Foreground shells already render inline; foreground Bats run for minutes
    // and stay listed so their progress is visible from the leader view.
    if(value::property(mission,"isBackgrounded")==false&&
        text(mission,"type")!="local_agent")continue;
    auto row=mission;row["id"]=it.key();rows.push_back(std::move(row));
  }
  std::stable_sort(rows.begin(),rows.end(),[](const Json& a,const Json& b){
    const bool left=text(a,"status")=="running",right=text(b,"status")=="running";
    if(left!=right)return left;
    return number(a,"startTime")>number(b,"startTime");
  });
  Json active=Json::array();
  for(auto& row:rows)active.push_back(std::move(row));
  return active;
}
std::size_t visibleFooterMissions(const Json& missions,int rows){
  return std::min(missions.size(),static_cast<std::size_t>(
    std::clamp(rows/4-1,1,4)));
}
ftxui::Element missionFooterAt(const Json& missions,int columns,int rows,
    ftxui::Color color,long long nowMs){
  ftxui::Elements lines;
  if(missions.empty())return ftxui::vbox(std::move(lines));
  lines.push_back(ftxui::text(clipped("Active Missions ("+
    std::to_string(missions.size())+") · Ctrl+T Bats · /missions",columns))|
    ftxui::color(color)|ftxui::dim);
  for(std::size_t i=0;i<visibleFooterMissions(missions,rows);++i){
    const auto& mission=missions[i];
    const auto glance=batGlance(mission,nowMs);
    std::string detail=glance.foreground?"foreground":glance.status;
    if(!glance.elapsed.empty())detail+=" · "+glance.elapsed;
    if(text(mission,"type")=="local_agent"||glance.gadgetUses>0)
      detail+=" · "+std::to_string(glance.gadgetUses)+
        (glance.gadgetUses==1?" Gadget":" Gadgets");
    if(!glance.gadget.empty())detail+=" · "+glance.gadget;
    if(!glance.lastText.empty())detail+=" · “"+glance.lastText+"”";
    lines.push_back(ftxui::hbox({ftxui::text("  ● ")|ftxui::color(color),
      ftxui::text(clipped(glance.label+" · "+detail,columns-4))}));
  }
  return ftxui::vbox(std::move(lines));
}
ftxui::Element missionFooter(const Json& missions,int columns,int rows,
    ftxui::Color color){
  return missionFooterAt(missions,columns,rows,color,platform::nowMillis());
}
}
