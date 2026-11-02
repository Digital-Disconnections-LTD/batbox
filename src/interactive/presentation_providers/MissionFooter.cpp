#include "interactive/presentation_providers/MissionFooter.hpp"
#include "runtime/Platform.hpp"
#include <ftxui/dom/elements.hpp>
#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>
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
namespace {
bool batMission(const Json& mission){
  return text(mission,"type")=="local_agent"&&text(mission,"agentType")!="main-session";
}
bool newerFirst(const Json& a,const Json& b){
  const bool left=text(a,"status")=="running",right=text(b,"status")=="running";
  if(left!=right)return left;
  return number(a,"startTime")>number(b,"startTime");
}
std::string rowId(const Json& row){return text(row,"id");}
}
std::optional<std::string> parentBatId(const Json& missions,const std::string& id){
  const auto& mission=value::property(missions,id.c_str());
  const auto parent=text(mission,"parentBatId");
  if(parent.empty()||parent==id||!batMission(value::property(missions,parent.c_str())))
    return std::nullopt;
  return parent;
}
Json backgroundFooterMissions(const Json& missions,const std::optional<std::string>& current){
  std::set<std::string> keep;
  if(current&&missions.is_object()){
    // The open Bat and the Bats above it stay listed so the highlight and the
    // way back to them survive a Bat settling.
    for(auto id=std::optional(*current);id&&keep.insert(*id).second;)
      id=parentBatId(missions,*id);
  }
  std::map<std::string,Json> bats;
  std::vector<Json> others;
  if(missions.is_object())for(auto it=missions.begin();it!=missions.end();++it){
    const auto& mission=it.value();
    if(text(mission,"agentType")=="main-session")continue;
    const bool listed=keep.contains(it.key())&&batMission(mission);
    if(!listed&&!active(text(mission,"status")))continue;
    // Foreground shells already render inline; foreground Bats run for minutes
    // and stay listed so their progress is visible from the leader view.
    if(value::property(mission,"isBackgrounded")==false&&
        text(mission,"type")!="local_agent")continue;
    auto row=mission;row["id"]=it.key();
    if(batMission(mission))bats.emplace(it.key(),std::move(row));
    else others.push_back(std::move(row));
  }
  std::map<std::string,std::vector<const Json*>> children;
  std::vector<const Json*> roots;
  for(const auto& [id,row]:bats){
    const auto parent=text(row,"parentBatId");
    if(!parent.empty()&&parent!=id&&bats.contains(parent))children[parent].push_back(&row);
    else roots.push_back(&row);
  }
  const auto order=[](std::vector<const Json*>& list){
    std::stable_sort(list.begin(),list.end(),[](const Json* a,const Json* b){
      return newerFirst(*a,*b);});
  };
  Json result=Json::array();
  std::set<std::string> placed;
  std::function<void(const Json*,int)> place=[&](const Json* row,int depth){
    const auto id=rowId(*row);
    if(!placed.insert(id).second)return;
    auto copy=*row;copy["depth"]=depth;result.push_back(std::move(copy));
    auto found=children.find(id);
    if(found==children.end())return;
    auto list=found->second;order(list);
    for(const auto* child:list)place(child,depth+1);
  };
  order(roots);
  for(const auto* root:roots)place(root,0);
  // A parent link that loops never reaches a root; list those Bats flat.
  for(const auto& [id,row]:bats)if(!placed.contains(id))place(&row,0);
  std::stable_sort(others.begin(),others.end(),newerFirst);
  for(auto& row:others)result.push_back(std::move(row));
  return result;
}
std::vector<std::string> footerBatIds(const Json& rows){
  std::vector<std::string> ids;
  if(rows.is_array())for(const auto& row:rows)
    if(batMission(row))ids.push_back(rowId(row));
  return ids;
}
std::vector<std::string> batTrail(const Json& rows,const std::string& id){
  std::map<std::string,const Json*> byId;
  if(rows.is_array())for(const auto& row:rows)if(batMission(row))byId.emplace(rowId(row),&row);
  std::vector<std::string> trail;
  std::set<std::string> seen;
  for(auto at=byId.find(id);at!=byId.end()&&seen.insert(at->first).second;
      at=byId.find(text(*at->second,"parentBatId"))){
    auto label=batGlance(*at->second,0).label;
    trail.push_back(label.empty()?at->first:label);
  }
  std::reverse(trail.begin(),trail.end());
  return trail;
}
FooterWindow footerWindow(const Json& rows,int terminalRows,bool collapsed,
    const std::optional<std::string>& current){
  FooterWindow window;
  const auto count=rows.is_array()?rows.size():std::size_t{0};
  const auto capacity=static_cast<std::size_t>(std::clamp(terminalRows/4-1,0,5));
  window.collapsed=collapsed||capacity<2;
  if(window.collapsed||count==0)return window;
  std::size_t selected=0;
  if(current)for(std::size_t i=0;i<count;++i)if(rowId(rows[i])==*current){selected=i;break;}
  window.begin=selected>=capacity?selected-capacity+1:0;
  window.end=std::min(count,window.begin+capacity);
  return window;
}
ftxui::Element missionFooterAt(const Json& missions,const FooterOptions& options){
  ftxui::Elements lines;
  if(!missions.is_array()||missions.empty())return ftxui::vbox(std::move(lines));
  const int columns=std::max(1,options.columns);
  const auto color=options.color;
  std::size_t running=0;
  std::string currentLabel;
  for(const auto& mission:missions){
    if(active(text(mission,"status")))++running;
    if(options.current&&rowId(mission)==*options.current)
      currentLabel=batGlance(mission,options.nowMs).label;
  }
  const auto window=footerWindow(missions,options.terminalRows,options.collapsed,
    options.current);
  const auto title="Active Missions ("+std::to_string(running)+")";
  if(window.collapsed){
    ftxui::Elements parts{ftxui::text(title)|ftxui::color(color)|ftxui::dim};
    if(!currentLabel.empty()){
      parts.push_back(ftxui::text(" · ")|ftxui::color(color)|ftxui::dim);
      parts.push_back(ftxui::text(clipped("in: "+currentLabel,
        std::max(3,columns-static_cast<int>(value::stringLength(title))-3)))|ftxui::color(color)|ftxui::bold);
    }
    return ftxui::hbox(std::move(parts));
  }
  lines.push_back(ftxui::text(clipped(title+" · Ctrl+T collapse · /missions",columns))|
    ftxui::color(color)|ftxui::dim);
  for(std::size_t i=window.begin;i<window.end;++i){
    const auto& mission=missions[i];
    const auto glance=batGlance(mission,options.nowMs);
    const bool here=options.current&&rowId(mission)==*options.current;
    std::string detail=glance.foreground?"foreground":
      glance.status=="killed"?"stopped":glance.status;
    if(!glance.elapsed.empty())detail+=" · "+glance.elapsed;
    if(text(mission,"type")=="local_agent"||glance.gadgetUses>0)
      detail+=" · "+std::to_string(glance.gadgetUses)+
        (glance.gadgetUses==1?" Gadget":" Gadgets");
    // A Gadget description can be a long path; keep room for the Bat's words.
    if(!glance.gadget.empty())detail+=" · "+clipped(glance.gadget,36);
    if(!glance.lastText.empty())detail+=" · “"+glance.lastText+"”";
    const auto depth=static_cast<int>(std::clamp(number(mission,"depth"),0LL,8LL));
    const std::string indent=depth>0?std::string(static_cast<std::size_t>(2*(depth-1)),' ')+"└ ":"";
    const int used=4+static_cast<int>(value::stringLength(indent));
    auto marker=ftxui::text(here?"▸ ":"  ")|ftxui::color(color);
    auto row=ftxui::text(clipped(glance.label+" · "+detail,std::max(3,columns-used)));
    if(here){marker=marker|ftxui::bold;row=row|ftxui::bold;}
    lines.push_back(ftxui::hbox({std::move(marker),ftxui::text(indent)|ftxui::dim,
      ftxui::text("● ")|ftxui::color(color),std::move(row)}));
  }
  return ftxui::vbox(std::move(lines));
}
ftxui::Element missionFooter(const Json& missions,FooterOptions options){
  options.nowMs=platform::nowMillis();
  return missionFooterAt(missions,options);
}
}
