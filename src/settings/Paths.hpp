#pragma once
#include "settings/Sources.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::settings {
struct PathServices {
  std::string processCwd,originalCwd,configHome,platform;
  std::optional<std::string> flagSettingsPath,managedPathOverride;
  bool coworkPlugins=false,coworkEnvironment=false,internalUser=false;
};
class Paths {
 public:
  explicit Paths(PathServices services);
  std::string getManagedFilePath();
  std::string getManagedSettingsDropInDir();
  std::string getManagedSettingsFilePath();
  std::string getSettingsRootPathForSource(Source source) const;
  std::string getUserSettingsFilePath() const;
  std::optional<std::string> getSettingsFilePathForSource(Source source);
  std::string getRelativeSettingsFilePathForSource(Source source) const;
 private:
  PathServices services_;
  std::optional<std::string> managedPath_,dropInDir_;
};
}
