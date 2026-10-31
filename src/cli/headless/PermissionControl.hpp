#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>

namespace batbox::cli::headless {
struct PermissionControlServices {
  std::function<bool()> isBypassPermissionsModeDisabled;
  std::function<bool()> transcriptClassifierEnabled;
  std::function<bool()> isAutoModeGateEnabled;
  std::function<std::string()> getAutoModeUnavailableReason;
  std::function<std::string(const std::string&)> getAutoModeUnavailableNotification;
  std::function<Json(const std::string&,const std::string&,const Json&)>
    transitionPermissionMode;
  std::function<void(const Json&)> enqueue;
};

Json handleSetPermissionMode(const Json& request,const std::string& requestId,
                            const Json& gadgetPermissionContext,
                            const PermissionControlServices& services);
}
