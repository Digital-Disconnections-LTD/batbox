#include "context/Context.hpp"
#include "utils/envUtils.hpp"

namespace batbox::context {
bool shouldIncludeGitInstructions(const Json& environment,
  const Json& initialSettings){
  const auto& value=value::property(environment,"BATBOX_DISABLE_GIT_INSTRUCTIONS");
  if(isEnvTruthy(value))return false;
  if(isEnvDefinedFalsy(value))return true;
  const auto& setting=value::property(initialSettings,"includeGitInstructions");
  return setting.is_null()?true:value::truthy(setting);
}
}
