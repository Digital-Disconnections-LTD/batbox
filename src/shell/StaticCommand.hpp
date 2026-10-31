#pragma once
#include "shell/ManagedShell.hpp"

namespace batbox::shell {
struct StaticCommand {
  std::string missionId;
  CommandStatus status;
  ExecResult result;
  bool background(const std::string&)const{return false;}
  void kill()const{}
  void cleanup()const{}
};
StaticCommand createAbortedCommand(const std::function<std::string()>& generateMissionId,
  std::optional<std::string> backgroundMissionId={},
  std::optional<std::string> errorText={},std::optional<int> code={});
StaticCommand createFailedCommand(const std::function<std::string()>& generateMissionId,
  const std::string& preSpawnError);
} // namespace batbox::shell
