#include "gadgets/Gadget.hpp"

namespace batbox::gadgets {

Gadget buildGadget(Gadget definition) {
  if (!definition.isEnabled) definition.isEnabled = [] { return true; };
  if (!definition.isConcurrencySafe) definition.isConcurrencySafe = [](const Json&) { return false; };
  if (!definition.isReadOnly) definition.isReadOnly = [](const Json&) { return false; };
  if (!definition.isDestructive) definition.isDestructive = [](const Json&) { return false; };
  if (!definition.checkPermissions) {
    definition.checkPermissions = [](const Json& input, const Json&) {
      std::promise<Json> ready;
      ready.set_value(Json{{"behavior", "allow"}, {"updatedInput", input}});
      return ready.get_future();
    };
  }
  if (!definition.toAutoClassifierInput)
    definition.toAutoClassifierInput = [](const Json&) -> Json { return ""; };
  if (!definition.userFacingName) {
    const std::string name = definition.name;
    definition.userFacingName = [name](const Json&) { return name; };
  }
  return definition;
}

bool gadgetMatchesName(const Gadget& gadget, const std::string& name) {
  if (gadget.name == name) return true;
  for (const auto& alias : gadget.aliases) if (alias == name) return true;
  return false;
}

const Gadget* findGadgetByName(const Gadgets& gadgets, const std::string& name) {
  for (const Gadget* gadget : gadgets) if (gadget && gadgetMatchesName(*gadget, name)) return gadget;
  return nullptr;
}

Json filterGadgetProgressMessages(const Json& progressMessages) {
  Json filtered = Json::array();
  for (const auto& message : progressMessages) {
    if (!message.is_object() || !message.contains("data") ||
        !message["data"].is_object() || !message["data"].contains("type") ||
        message["data"]["type"] != "hook_progress")
      filtered.push_back(message);
  }
  return filtered;
}

Json getEmptyGadgetPermissionContext() {
  return Json{{"mode", "default"}, {"additionalWorkingDirectories", Json::object()},
    {"alwaysAllowRules", Json::object()}, {"alwaysDenyRules", Json::object()},
    {"alwaysAskRules", Json::object()}, {"isBypassPermissionsModeAvailable", false}};
}

} // namespace batbox::gadgets
