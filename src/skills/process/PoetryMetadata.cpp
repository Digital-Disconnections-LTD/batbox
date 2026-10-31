#include "skills/process/PoetryCommand.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::skills::process {
std::string formatSkillLoadingMetadata(const std::string& name,
  const std::string& progressMessage){
  (void)progressMessage;
  return "<command-message>"+name+"</command-message>\n"+
    "<command-name>"+name+"</command-name>\n<skill-format>true</skill-format>";
}
std::string formatSlashCommandLoadingMetadata(const std::string& name,
  const std::optional<std::string>& args){
  std::string text="<command-message>"+name+"</command-message>\n"+
    "<command-name>/"+name+"</command-name>";
  if(args&&!args->empty())text+="\n<command-args>"+*args+"</command-args>";
  return text;
}
std::string formatCommandLoadingMetadata(const Json& command,
  const std::optional<std::string>& args){
  const auto& rawName=value::property(command,"name");
  const auto name=rawName.is_string()?rawName.get<std::string>():std::string{};
  if(value::property(command,"userInvocable")!=false)
    return formatSlashCommandLoadingMetadata(name,args);
  const auto& loaded=value::property(command,"loadedFrom");
  if(loaded=="skills"||loaded=="plugin"||loaded=="mcp"){
    const auto& progress=value::property(command,"progressMessage");
    return formatSkillLoadingMetadata(name,progress.is_string()?progress.get<std::string>():"");
  }
  return formatSlashCommandLoadingMetadata(name,args);
}
}
