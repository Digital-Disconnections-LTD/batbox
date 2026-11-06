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
std::unordered_set<std::string> runningBatGadgetUses(const Json& rows){
  std::unordered_set<std::string> uses;
  if(rows.is_array())for(const auto& row:rows){
    const auto use=text(row,"toolUseId");
    if(batMission(row)&&active(text(row,"status"))&&!use.empty())uses.insert(use);
  }
  return uses;
}
std::vector<std::string> footerBatIds(const Json& rows){
  std::vector<std::string> ids;
  // A Bat waiting for a slot has no conversation to open yet.
  if(rows.is_array())for(const auto& row:rows)
    if(batMission(row)&&value::property(row,"waiting")!=true)ids.push_back(rowId(row));
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
    const std::optional<std::string>& current,int extraRows){
  FooterWindow window;
  const auto count=rows.is_array()?rows.size():std::size_t{0};
  const auto capacity=static_cast<std::size_t>(std::clamp(terminalRows/4-1-extraRows,0,5));
  window.collapsed=collapsed||capacity<2;
  if(window.collapsed||count==0)return window;
  std::size_t selected=0;
  if(current)for(std::size_t i=0;i<count;++i)if(rowId(rows[i])==*current){selected=i;break;}
  window.begin=selected>=capacity?selected-capacity+1:0;
  window.end=std::min(count,window.begin+capacity);
  return window;
}
namespace {
std::string padded(const std::string& value,std::size_t width){
  const auto length=value::stringLength(value);
  return length>=width?value:value+std::string(width-length,' ');
}
std::string leftPadded(const std::string& value,std::size_t width){
  const auto length=value::stringLength(value);
  return length>=width?value:std::string(width-length,' ')+value;
}
// One Active Missions row split into its columns.
struct FooterRow {
  std::string marker,indent,status,title,activity,state,elapsed,gadgets,tokens;
  bool here=false,selected=false,waiting=false;
};
FooterRow footerRow(const Json& mission,const FooterOptions& options){
  const auto glance=batGlance(mission,options.nowMs);
  FooterRow row;
  const auto id=rowId(mission);
  row.here=options.current&&id==*options.current;
  row.selected=options.selected&&id==*options.selected;
  row.waiting=value::property(mission,"waiting")==true||glance.status=="pending";
  row.marker=options.selected?(row.selected?"❯ ":"  "):row.here?"▸ ":"  ";
  const auto depth=static_cast<int>(std::clamp(number(mission,"depth"),0LL,8LL));
  row.indent=depth>0?std::string(static_cast<std::size_t>(2*(depth-1)),' ')+"└ ":"";
  row.status=row.waiting?"○ ":"● ";
  row.title=glance.label;
  const auto newline=row.title.find_first_of("\r\n");
  if(newline!=std::string::npos)row.title.resize(newline);
  row.activity=row.waiting?std::string():glance.gadget;
  const auto activityEnd=row.activity.find_first_of("\r\n");
  if(activityEnd!=std::string::npos)row.activity.resize(activityEnd);
  row.state=row.waiting?"waiting":glance.foreground?"foreground":
    glance.status=="killed"?"stopped":glance.status;
  if(!row.waiting)row.elapsed=glance.elapsed;
  if(!row.waiting&&(text(mission,"type")=="local_agent"||glance.gadgetUses>0))
    row.gadgets=std::to_string(glance.gadgetUses)+(glance.gadgetUses==1?" Gadget":" Gadgets");
  if(!row.waiting&&text(mission,"type")=="local_agent")row.tokens=compactTokens(glance.tokens);
  return row;
}
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
  const bool focused=options.selected.has_value();
  const auto window=footerWindow(missions,options.terminalRows,options.collapsed,
    focused&&!options.selected->empty()?options.selected:options.current,focused?1:0);
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
  const auto hint=focused?" · Enter view · x stop · Esc back":
    footerBatIds(missions).empty()?" · Ctrl+T collapse · /missions":
    " · ↓ select · Ctrl+T collapse · /missions";
  lines.push_back(ftxui::text(clipped(title+hint,columns))|ftxui::color(color)|ftxui::dim);
  std::vector<FooterRow> rows;
  if(focused){
    // The main conversation leads the list while it has focus.
    FooterRow flight;
    flight.selected=options.selected->empty();
    flight.here=!options.current;
    flight.marker=flight.selected?"❯ ":"  ";
    flight.status=flight.here?"● ":"○ ";
    flight.title="Flight";
    rows.push_back(std::move(flight));
  }
  for(std::size_t i=window.begin;i<window.end;++i)rows.push_back(footerRow(missions[i],options));
  // Columns line up across the visible rows. The figures on the right keep
  // their width; when the row does not fit, the activity goes first, then
  // the Gadget count, then the elapsed time. Title, status and tokens stay.
  std::size_t lead=0,titleWidth=0,activityWidth=0,stateWidth=0,elapsedWidth=0,
    gadgetWidth=0,tokenWidth=0;
  for(const auto& row:rows){
    lead=std::max(lead,2+value::stringLength(row.indent)+2);
    titleWidth=std::max(titleWidth,value::stringLength(row.title));
    activityWidth=std::max(activityWidth,value::stringLength(row.activity));
    stateWidth=std::max(stateWidth,value::stringLength(row.state));
    elapsedWidth=std::max(elapsedWidth,value::stringLength(row.elapsed));
    gadgetWidth=std::max(gadgetWidth,value::stringLength(row.gadgets));
    tokenWidth=std::max(tokenWidth,value::stringLength(row.tokens));
  }
  titleWidth=std::min<std::size_t>(titleWidth,40);
  activityWidth=std::min<std::size_t>(activityWidth,36);
  bool showActivity=activityWidth>0,showGadgets=gadgetWidth>0,showElapsed=elapsedWidth>0;
  const auto right=[&]{
    std::size_t width=2+stateWidth;
    if(showElapsed)width+=2+elapsedWidth;
    if(showGadgets)width+=2+gadgetWidth;
    if(tokenWidth)width+=2+tokenWidth;
    return width;
  };
  const std::size_t minimumTitle=12;
  const auto available=static_cast<std::size_t>(columns);
  const auto fits=[&](std::size_t titleColumns){
    return lead+titleColumns+(showActivity?2+activityWidth:0)+right()<=available;
  };
  const auto least=std::min(titleWidth,minimumTitle);
  if(showActivity&&!fits(least))showActivity=false;
  if(showGadgets&&!fits(least))showGadgets=false;
  if(showElapsed&&!fits(least))showElapsed=false;
  const auto used=lead+(showActivity?2+activityWidth:0)+right();
  const auto room=available>used?available-used:std::size_t{3};
  const auto shownTitle=std::max<std::size_t>(std::min(titleWidth,room),3);
  // Whatever room is left goes to the activity, up to its own width.
  std::size_t shownActivity=0;
  if(showActivity){
    const auto spare=available>lead+shownTitle+2+right()?
      available-lead-shownTitle-2-right():std::size_t{0};
    shownActivity=std::min(spare,activityWidth);
  }
  for(const auto& row:rows){
    const auto cut=[](const std::string& value,std::size_t width){
      return clipped(value,static_cast<int>(width));};
    std::string figures=leftPadded(row.state,stateWidth);
    if(showElapsed)figures+="  "+leftPadded(row.elapsed,elapsedWidth);
    if(showGadgets)figures+="  "+leftPadded(row.gadgets,gadgetWidth);
    if(tokenWidth)figures+="  "+leftPadded(row.tokens,tokenWidth);
    const auto head=row.marker+row.indent;
    ftxui::Elements parts;
    parts.push_back(ftxui::text(row.marker)|ftxui::color(color)|
      (row.here?ftxui::bold:ftxui::nothing));
    parts.push_back(ftxui::text(row.indent)|ftxui::dim);
    parts.push_back(ftxui::text(row.status)|ftxui::color(color)|
      (row.waiting?ftxui::dim:ftxui::nothing));
    // A deeper row's title starts further right; its column ends where the
    // others do, so the columns after it line up.
    const auto titleColumns=shownTitle+(lead-value::stringLength(head)-2);
    auto name=ftxui::text(padded(cut(row.title,titleColumns),titleColumns));
    if(row.here)name=name|ftxui::bold;
    parts.push_back(std::move(name));
    if(showActivity&&shownActivity>0)
      parts.push_back(ftxui::text("  "+padded(cut(row.activity,shownActivity),shownActivity))|
        ftxui::dim);
    parts.push_back(ftxui::filler());
    parts.push_back(ftxui::text("  "+figures)|(row.waiting?ftxui::dim:ftxui::nothing));
    auto line=ftxui::hbox(std::move(parts));
    // With the list focused only the focused row is bright.
    if(focused)line=row.selected?line|ftxui::bold:line|ftxui::dim;
    lines.push_back(std::move(line));
  }
  return ftxui::vbox(std::move(lines));
}
ftxui::Element missionFooter(const Json& missions,FooterOptions options){
  options.nowMs=platform::nowMillis();
  return missionFooterAt(missions,options);
}
}
