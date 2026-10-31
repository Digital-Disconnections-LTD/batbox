#pragma once
#include "settings/Sources.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>

namespace batbox::settings::persistence {
struct TrustedModeServices {
  std::function<Json(Source)> getSettingsForSource;
  std::function<void(const std::string&)> debug;
  bool transcriptClassifier=false;
  bool internalUser=false;
};
bool hasSkipDangerousModePermissionPoetry(const TrustedModeServices& services);
bool hasAutoModeOptIn(const TrustedModeServices& services);
bool getUseAutoModeDuringPlan(const TrustedModeServices& services);
std::optional<Json> getAutoModeConfig(const TrustedModeServices& services);
}
