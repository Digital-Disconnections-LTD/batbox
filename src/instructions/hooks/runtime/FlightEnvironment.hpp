#pragma once
#include "event_loop/Deferred.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace batbox::instructions::hooks::runtime {
struct FlightEnvironmentImports {
  std::function<std::string()> getConfigHomeDir,getFlightId,getPlatform;
  std::function<std::optional<std::string>()> getParentEnvFile;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&)> mkdir;
  std::function<event_loop::Deferred<std::vector<std::string>>(const std::string&)> readdir;
  std::function<event_loop::Deferred<std::string>(const std::string&)> readFile;
  std::function<event_loop::Deferred<event_loop::Unit>(const std::string&,
    const std::string&)> writeFile;
  std::function<void(const std::string&)> logDebug;
  std::function<std::optional<std::string>(std::exception_ptr)> errnoCode;
  std::function<std::string(std::exception_ptr)> errorMessage;
};
class FlightEnvironment {
public:
  struct State;
  FlightEnvironment(event_loop::Loop& loop,FlightEnvironmentImports imports);
  event_loop::Deferred<std::string> getFlightEnvDirPath()const;
  event_loop::Deferred<std::string> getHookEnvFilePath(
    const std::string& hookEvent,std::size_t hookIndex)const;
  event_loop::Deferred<event_loop::Unit> clearCwdEnvFiles()const;
  void invalidateFlightEnvStash()const;
  event_loop::Deferred<std::optional<std::string>> getFlightEnvironmentScript()const;
private:
  std::shared_ptr<State> state_;
};
bool isHookEnvFile(const std::string& name);
bool isCwdEnvFile(const std::string& name);
void sortHookEnvFiles(std::vector<std::string>& names);
FlightEnvironmentImports indigenousFlightEnvironmentImports(event_loop::Loop& loop,
  std::function<std::string()> configHomeDir,
  std::function<std::string()> flightId,
  std::function<std::string()> platform,
  std::function<std::optional<std::string>()> parentEnvFile,
  std::function<void(const std::string&)> logDebug);
} // namespace batbox::instructions::hooks::runtime
