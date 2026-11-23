#include "compaction/Compaction.hpp"
#include "compaction/FlightPolicy.hpp"
#include <iostream>
using namespace batbox;
using namespace batbox::compaction;
namespace {
Json toolUse(const std::string& id){
  return Json{{"type","tool_use"},{"id",id},{"name","Read"},
    {"input",Json{{"file_path","x"}}}};
}
Json toolResult(const std::string& id,const std::string& content){
  return Json{{"type","tool_result"},{"tool_use_id",id},{"content",content}};
}
Json assistantMessage(const Json& block){
  return Json{{"type","assistant"},{"message",Json{{"content",Json::array({block})}}}};
}
Json userMessage(const Json& block){
  return Json{{"type","user"},{"message",Json{{"content",Json::array({block})}}}};
}
double contentTokens(const Json& block){
  const auto& content=value::property(block,"content");
  return content.is_string()?double(content.get<std::string>().size())/4:0;
}
Json thread(std::size_t toolCalls){
  Json messages=Json::array();
  const std::string payload(800,'x');
  for(std::size_t i=0;i<toolCalls;++i){
    const auto id="t"+std::to_string(i);
    messages.push_back(assistantMessage(toolUse(id)));
    messages.push_back(userMessage(toolResult(id,payload)));
  }
  return messages;
}
void report(const char* what){std::cerr<<what<<"\n";}
}
int main(){
  if(flightKind("agent:builtin:general-purpose")!=FlightKind::Dispatch||
     flightKind("agent:custom:npu-probe")!=FlightKind::Dispatch){
    report("dispatch sources are not Dispatch");return 1;
  }
  if(flightKind("repl_main_thread")!=FlightKind::Main||
     flightKind("sdk")!=FlightKind::Main||flightKind("hook_agent")!=FlightKind::Main||
     flightKind("")!=FlightKind::Main){
    report("main and unknown sources are not Main");return 1;
  }
  if(flightKind("compact")!=FlightKind::Auxiliary||
     flightKind("session_memory")!=FlightKind::Auxiliary){
    report("compact and session_memory are not Auxiliary");return 1;
  }
  const auto& dispatch=flightPolicy("agent:custom:npu-probe");
  const auto& mainFlight=flightPolicy("repl_main_thread");
  if(dispatch.llmAutoCompact||!dispatch.budgetMicrocompact||
     dispatch.keepRecentToolResults!=3||!mainFlight.llmAutoCompact||
     mainFlight.budgetMicrocompact){
    report("dispatch policy: no LLM summary, budget pruning, keep 3");return 1;
  }
  AutoServices services;
  services.isAutoCompactEnabled=[]{return true;};
  services.tokenCountWithEstimation=[](const Json&){return 60000.0;};
  services.calculateTokenWarningState=[](double,const std::string&){
    return Json{{"isAboveAutoAutoCompactThreshold",true},
      {"isAboveAutoCompactThreshold",true}};};
  const Json empty=Json::array();
  if(shouldAutoCompact(empty,"m","agent:builtin:x",services)||
     shouldAutoCompact(empty,"m","compact",services)||
     !shouldAutoCompact(empty,"m","repl_main_thread",services)||
     !shouldAutoCompact(empty,"m",std::nullopt,services)){
    report("shouldAutoCompact must refuse Dispatch and Auxiliary, keep Main");return 1;
  }
  const auto messages=thread(5);
  const auto pruned=pruneOldToolResults(messages,2,contentTokens);
  if(!pruned.changed||pruned.cleared!=3||pruned.tokensSaved<=0){
    report("prune must fold the oldest three of five and keep the last two");return 1;
  }
  std::size_t folded=0,intact=0;
  for(const auto& message:pruned.messages)
    if(value::stringEquals(value::property(message,"type"),"user"))
      for(const auto& block:value::property(value::property(message,"message"),"content"))
        if(value::stringEquals(value::property(block,"type"),"tool_result")){
          const auto& content=value::property(block,"content");
          if(content.is_string()&&content.get<std::string>()==
              "[Old tool result content cleared]")++folded;else ++intact;
        }
  if(folded!=3||intact!=2||pruned.messages.size()!=messages.size()){
    report("folded and kept counts must match the policy");return 1;
  }
  if(messages!=thread(5)){
    report("pruning must not modify the input messages");return 1;
  }
  if(pruneOldToolResults(messages,5,contentTokens).changed){
    report("keep covering every result must leave the thread unchanged");return 1;
  }
  if(pruneOldToolResults(pruned.messages,2,contentTokens).changed){
    report("re-pruning already-folded results must be unchanged");return 1;
  }
  std::cout<<"Flight-class policy registry routes dispatch lanes away from LLM\n"
    "compaction, and pruneOldToolResults folds only older compactable results\n";
}
