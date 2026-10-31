#include "interactive/repl_runtime/Runtime.hpp"

namespace batbox::interactive::repl_runtime {
std::optional<std::string> stopHookSpinnerSuffix(
    const TurnMessagesRef& messages,bool isLoading){
  if(!isLoading||!messages)return {};
  std::vector<TurnMessageRef> progress;
  for(const auto& message:*messages){
    if(!message)continue;
    const auto& data=value::property(*message,"data");
    const auto& event=value::property(data,"hookEvent");
    if(value::property(*message,"type")=="progress"&&
        value::property(data,"type")=="hook_progress"&&
        (event=="Stop"||event=="SubagentStop"))
      progress.push_back(message);
  }
  if(progress.empty())return {};
  const auto& gadgetUseId=value::property(*progress.back(),"toolUseID");
  if(!value::truthy(gadgetUseId))return {};
  for(const auto& message:*messages){
    if(message&&value::property(*message,"type")=="system"&&
        value::property(*message,"subtype")=="stop_hook_summary"&&
        value::property(*message,"toolUseID")==gadgetUseId)return {};
  }
  std::vector<TurnMessageRef> current;
  for(const auto& message:progress)
    if(value::property(*message,"toolUseID")==gadgetUseId)
      current.push_back(message);
  std::size_t completed=0;
  for(const auto& message:*messages){
    if(!message||value::property(*message,"type")!="attachment")continue;
    const auto& attachment=value::property(*message,"attachment");
    const auto& event=value::property(attachment,"hookEvent");
    if((event=="Stop"||event=="SubagentStop")&&
        value::property(attachment,"toolUseID")==gadgetUseId)
      ++completed;
  }
  for(const auto& message:current){
    const auto& status=value::property(value::property(*message,"data"),
      "statusMessage");
    if(value::truthy(status)){
      const auto text=status.is_string()?status.get<std::string>():
        value::stringify(status);
      return current.size()==1?text+"…":text+"… "+
        std::to_string(completed)+"/"+std::to_string(current.size());
    }
  }
  const auto& event=value::property(value::property(*current.front(),"data"),
    "hookEvent");
  const std::string hookType=event=="SubagentStop"?"subagent stop":"stop";
  if(current.size()==1)return "running "+hookType+" hook";
  return "running stop hooks… "+std::to_string(completed)+"/"+
    std::to_string(current.size());
}
} // namespace batbox::interactive::repl_runtime
