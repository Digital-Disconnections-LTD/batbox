#include "interactive/cards_collapsed/CollapsedPlan.hpp"
#include "interactive/MessageDecisions.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace batbox::interactive::cards_collapsed {
namespace {
void require(const auto& fn,const char* name){
  if(!fn)throw std::logic_error(std::string(name)+" import required");
}
double count(const Json& object,const char* key){
  const auto it=object.find(key);
  return it==object.end()||it->is_null()?0.0:it->get<double>();
}
double maxJS(double a,double b){
  return std::isnan(a)||std::isnan(b)?std::numeric_limits<double>::quiet_NaN():
    std::max(a,b);
}
std::optional<std::string> lastString(const Json& object,const char* key){
  const auto it=object.find(key);
  if(it==object.end()||!it->is_array()||it->empty())return {};
  return it->back().is_string()?std::optional<std::string>(it->back().get<std::string>()):
    std::nullopt;
}
std::optional<Json> latestProgress(const MessageLookups& lookups,
                                  const std::string& id){
  const auto found=lookups.progressMessagesByGadgetUseID.find(id);
  if(found==lookups.progressMessagesByGadgetUseID.end()||found->second.empty()||
     !found->second.back())return {};
  return *found->second.back();
}
std::string fieldString(const Json& object,const char* key){
  const auto it=object.find(key);
  return it!=object.end()&&it->is_string()?it->get<std::string>():"";
}
std::vector<std::string> splitLines(const std::string& text){
  std::vector<std::string> lines;
  std::size_t begin=0;
  for(;;){
    const auto end=text.find('\n',begin);
    if(end==std::string::npos){lines.push_back(text.substr(begin));break;}
    lines.push_back(text.substr(begin,end-begin));begin=end+1;
  }
  return lines;
}
}

CollapsedCard classifyCollapsedReadSearch(CollapsedState& state,
   const CollapsedInputs& input,const CollapsedServices& services){
  if(!input.lookups||!input.inProgressGadgetUseIDs||!input.gadgets)
    throw std::logic_error("Collapsed group lookups, active IDs and Gadgets required");
  const auto& message=input.message;
  require(services.readSelectedBackground,"selected message background");
  CollapsedCard card;
  card.sourceMessage=message;
  card.shouldAnimate=input.shouldAnimate;
  card.selectedBackground=services.readSelectedBackground();
  const double rawSearch=count(message,"searchCount");
  const double rawRead=count(message,"readCount");
  const double rawList=count(message,"listCount");
  const double repl=count(message,"replCount");
  const double memorySearch=count(message,"memorySearchCount");
  const double memoryRead=count(message,"memoryReadCount");
  const double memoryWrite=count(message,"memoryWriteCount");
  require(services.readTheme,"theme");
  card.theme=services.readTheme();
  const auto gadgetUseIDs=getGadgetUseIdsFromCollapsedGroup(message);
  for(const auto& id:gadgetUseIDs)
    card.error|=input.lookups->erroredGadgetUseIDs.contains(id);
  const bool hasMemory=memorySearch>0||memoryRead>0||memoryWrite>0;
  require(services.teamMemoryEnabled,"team memory feature");
  const bool teamEnabled=services.teamMemoryEnabled();
  const bool hasTeam=teamEnabled&&(count(message,"teamMemorySearchCount")>0||
    count(message,"teamMemoryReadCount")>0||count(message,"teamMemoryWriteCount")>0);
  state.maximum.read=maxJS(state.maximum.read,rawRead);
  state.maximum.search=maxJS(state.maximum.search,rawSearch);
  state.maximum.list=maxJS(state.maximum.list,rawList);
  state.maximum.mcp=maxJS(state.maximum.mcp,count(message,"mcpCallCount"));
  state.maximum.bash=maxJS(state.maximum.bash,count(message,"bashCount"));
  require(services.isFullscreen,"fullscreen environment");
  const bool fullscreen=services.isFullscreen();
  const double gitBash=count(message,"gitOpBashCount");
  const double bash=fullscreen?std::max(0.0,state.maximum.bash-gitBash):0.0;
  const bool hasNonMemory=state.maximum.search>0||state.maximum.read>0||
    state.maximum.list>0||repl>0||state.maximum.mcp>0||bash>0||gitBash>0;
  auto incoming=message.contains("latestDisplayHint")&&
    message["latestDisplayHint"].is_string()?
    std::optional<std::string>(message["latestDisplayHint"].get<std::string>()):
    std::nullopt;
  if(!message.contains("latestDisplayHint")){
    auto search=lastString(message,"searchArgs");
    const auto read=lastString(message,"readFilePaths");
    if(read){require(services.displayPath,"display path");
      incoming=services.displayPath(*read);
    }else if(search)incoming="\""+*search+"\"";
  }
  if(input.active){
    for(const auto& id:gadgetUseIDs){
      if(!input.inProgressGadgetUseIDs->contains(id))continue;
      const auto progress=latestProgress(*input.lookups,id);
      if(!progress||!progress->is_object()||!progress->contains("data"))continue;
      const auto& data=(*progress)["data"];
      if(!data.is_object()||data.value("type","")!="repl_tool_call"||
         data.value("phase","")!="start")continue;
      const Json gadgetInput=data.value("toolInput",Json::object());
      const auto path=fieldString(gadgetInput,"file_path");
      const auto pattern=fieldString(gadgetInput,"pattern");
      const auto command=fieldString(gadgetInput,"command");
      if(!path.empty())incoming=path;
      else if(!pattern.empty())incoming="\""+pattern+"\"";
      else if(!command.empty())incoming=command;
      else incoming=fieldString(data,"toolName");
    }
  }
  require(services.minimumDisplayTime,"minimum hint display time");
  const auto hint=services.minimumDisplayTime(incoming,700);
  card.active=input.active;card.verbose=input.verbose;
  if(input.verbose){
    // Verbose gadget rows and their result schemas are a separate child binding.
    const auto appendGadgetUse=[&](const Json& candidate){
      if(!candidate.is_object()||!candidate.contains("message"))return;
      const auto& inner=candidate["message"];
      if(!inner.is_object()||!inner.contains("content"))return;
      const auto& content=inner["content"];
      if(content.is_array()&&!content.empty()&&content[0].is_object()&&
         content[0].value("type","")=="tool_use")
        card.verboseGadgetUses.push_back(candidate);
    };
    const auto it=message.find("messages");
    if(it!=message.end()&&it->is_array()){
      for(const auto& child:*it){
        if(child.value("type","")=="assistant")appendGadgetUse(child);
        else if(child.value("type","")=="grouped_tool_use"&&
                child.contains("messages")&&child["messages"].is_array())
          for(const auto& grouped:child["messages"])appendGadgetUse(grouped);
      }
    }
    card.visible=true;
    card.hookInfos=message.value("hookInfos",Json());
    const auto memories=message.find("relevantMemories");
    if(memories!=message.end()&&memories->is_array())
      card.relevantMemories=memories->get<std::vector<Json>>();
    return card;
  }
  if(!hasMemory&&!hasTeam&&!hasNonMemory)return card;
  card.visible=true;
  std::string shellSuffix;
  if(fullscreen&&input.active){
    std::optional<double> elapsed;
    double lines=0;
    for(const auto& id:gadgetUseIDs){
      if(!input.inProgressGadgetUseIDs->contains(id))continue;
      const auto progress=latestProgress(*input.lookups,id);
      if(!progress||!progress->is_object()||!progress->contains("data"))continue;
      const auto& data=(*progress)["data"];
      if(!data.is_object())continue;
      const auto type=data.value("type","");
      if(type!="bash_progress"&&type!="powershell_progress")continue;
      const double candidate=count(data,"elapsedTimeSeconds");
      if(!elapsed||candidate>*elapsed){elapsed=candidate;lines=count(data,"totalLines");}
    }
    if(elapsed&&*elapsed>=2){
      require(services.formatDuration,"duration formatter");
      const auto time=services.formatDuration(*elapsed*1000);
      shellSuffix=lines>0?" ("+time+" · "+value::numberToString(lines)+" "+
        (lines==1?"line":"lines")+")":" ("+time+")";
    }
  }
  appendCollapsedSummary(card.summary,message,state.maximum,fullscreen,
                         input.active,teamEnabled,services);
  require(services.expandHint,"expand shortcut");
  card.expandHint=services.expandHint();
  if(input.active&&hint){
    card.hintLines=splitLines(*hint);
    if(!card.hintLines.empty())card.hintLines.back()+=shellSuffix;
  }
  const double hookMs=count(message,"hookTotalMs");
  if(message.contains("hookTotalMs")&&hookMs>0){
    require(services.formatSecondsShort,"short seconds formatter");
    card.hookSummary="Ran "+value::numberToString(count(message,"hookCount"))+
      " PreToolUse "+(count(message,"hookCount")==1?"hook":"hooks")+
      " ("+services.formatSecondsShort(hookMs)+")";
  }
  return card;
}
}
