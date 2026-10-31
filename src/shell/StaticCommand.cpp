#include "shell/StaticCommand.hpp"
#include <stdexcept>
namespace batbox::shell {
StaticCommand createAbortedCommand(const std::function<std::string()>& generateMissionId,
  std::optional<std::string> backgroundMissionId,std::optional<std::string> errorText,
  std::optional<int> code){
  if(!generateMissionId)throw std::invalid_argument("generateTaskId required");
  StaticCommand command;command.missionId=generateMissionId();command.status=CommandStatus::Killed;
  command.result.code=code.value_or(145);
  command.result.stderrText=errorText.value_or("Command aborted before execution");
  command.result.interrupted=true;command.result.backgroundMissionId=std::move(backgroundMissionId);
  return command;
}
StaticCommand createFailedCommand(const std::function<std::string()>& generateMissionId,
  const std::string& preSpawnError){
  if(!generateMissionId)throw std::invalid_argument("generateTaskId required");
  StaticCommand command;command.missionId=generateMissionId();command.status=CommandStatus::Completed;
  command.result.code=1;command.result.stderrText=preSpawnError;
  command.result.preSpawnError=preSpawnError;
  return command;
}
} // namespace batbox::shell
