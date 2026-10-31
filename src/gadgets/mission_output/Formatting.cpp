#include "gadgets/mission_output/MissionOutput.hpp"

namespace batbox::mission_output {
Json formatMissionOutput(const std::string& output,const std::string& missionId,
  std::size_t maxLength,const std::function<std::string(const std::string&)>& getMissionOutputPath){
  if(value::stringLength(output)<=maxLength)
    return {{"content",output},{"wasTruncated",false}};
  if(!getMissionOutputPath)throw std::logic_error("getTaskOutputPath dependency not supplied");
  const auto path=getMissionOutputPath(missionId);
  const auto header="[Truncated. Full output: "+path+"]\n\n";
  const auto available=static_cast<double>(maxLength)-static_cast<double>(value::stringLength(header));
  const auto truncated=value::slice(output,-available);
  return {{"content",header+truncated},{"wasTruncated",true}};
}
namespace {
std::string stringField(const Json& value,const char* name){
  return value.is_object()&&value.contains(name)&&value[name].is_string()?value[name].get<std::string>():"";
}
std::string trimEnd(const std::string& value){
  const auto units=value::utf16CodeUnits(value);std::size_t end=units.size();
  while(end>0&&value::trim(value::fromUtf16(std::u16string_view(&units[end-1],1))).empty())--end;
  return value::fromUtf16(std::u16string_view(units.data(),end));
}
}
Json mapGadgetResult(const Json& data,const std::string& gadgetUseID,
  std::size_t maxLength,const std::function<std::string(const std::string&)>& getMissionOutputPath){
  std::vector<std::string> parts;
  parts.push_back("<retrieval_status>"+stringField(data,"retrieval_status")+"</retrieval_status>");
  if(data.is_object()&&data.contains("task")&&data["task"].is_object()){
    const auto& mission=data["task"];
    parts.push_back("<task_id>"+stringField(mission,"task_id")+"</task_id>");
    parts.push_back("<task_type>"+stringField(mission,"task_type")+"</task_type>");
    parts.push_back("<status>"+stringField(mission,"status")+"</status>");
    if(mission.contains("exitCode")&&!mission["exitCode"].is_null())
      parts.push_back("<exit_code>"+value::numberToString(mission["exitCode"].get<double>())+"</exit_code>");
    const auto output=stringField(mission,"output");
    if(!value::trim(output).empty()){
      const auto formatted=formatMissionOutput(output,stringField(mission,"task_id"),maxLength,getMissionOutputPath);
      parts.push_back("<output>\n"+trimEnd(formatted["content"].get<std::string>())+"\n</output>");
    }
    const auto error=stringField(mission,"error");
    if(!error.empty())parts.push_back("<error>"+error+"</error>");
  }
  std::string content;
  for(const auto& part:parts){if(!content.empty())content+="\n\n";content+=part;}
  return {{"tool_use_id",gadgetUseID},{"type","tool_result"},{"content",content}};
}
} // namespace batbox::mission_output
