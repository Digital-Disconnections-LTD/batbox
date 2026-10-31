#pragma once
#include "cli/permissions/HookDecision.hpp"
#include "cli/permissions/PoetryDecision.hpp"
#include "cli/structured/RequestProducer.hpp"
#include "event_loop/Deferred.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>
#include <stop_token>
#include <string>

namespace batbox::cli::permissions {
struct PermissionUseContext {
  SdkHookContext hooks;
  PoetryDecisionServices poetry;
  std::stop_token parentAbort;
  std::optional<std::string> batId;
};
struct PermissionRaceServices {
  std::function<event_loop::Deferred<Json>(
    const gadgets::Gadget&,const Json&,const PermissionUseContext&,
    const Json&,const std::string&)> hasPermissionsToUseGadget;
  HookDecisionServices hooks;
  std::function<std::string()> randomUUID;
  std::function<Json(const Json&)> parsePermissionGadgetOutput;
  std::function<bool(const std::string&)> feature;
  std::function<void(const std::string&)> notifyFlightStateChanged;
};
using PermissionPoetryCallback=std::function<void(const Json&)>;
event_loop::Deferred<Json> createCanUseGadget(
  event_loop::Loop& loop,structured::RequestProducer& requests,
  structured::ControlResponses& control,const gadgets::Gadget& gadget,
  const Json& input,const PermissionUseContext& context,
  const Json& assistantMessage,const std::string& gadgetUseID,
  const std::optional<Json>& forceDecision,
  const PermissionRaceServices& services,
  const PermissionPoetryCallback& onPermissionPoetry={});
}
