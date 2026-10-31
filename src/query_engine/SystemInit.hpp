#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>
namespace batbox::query_engine {
struct SystemInitInputs {
  Json gadgets=Json::array(),mcpClients=Json::array();
  std::string model,permissionMode;
  Json commands=Json::array(),bats=Json::array(),skills=Json::array(),plugins=Json::array();
  std::optional<bool> fastMode;
};
struct SystemInitServices {
  std::function<Json()> getSettings,getSdkBetas;
  std::function<std::string()> getCwd,getFlightId,getApiKeySource,randomUUID;
  std::function<bool(const std::string&)> feature;
  std::function<std::string()> getMessagingSocketPath;
  std::function<std::optional<std::string>(const std::string&,std::optional<bool>)> getFastModeState;
  std::string version;
  std::string defaultOutputStyle="default";
};
std::string sdkCompatGadgetName(const std::string& name);
Json buildSystemInitMessage(const SystemInitInputs& inputs,const SystemInitServices& services);
}
