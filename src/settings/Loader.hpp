#pragma once
#include "settings/Managed.hpp"
#include <functional>
#include <optional>

namespace batbox::settings {
struct LoadServices {
  Paths* paths=nullptr;
  Stash* stash=nullptr;
  ParseServices parser;
  std::vector<Source> allowedSources;
  std::function<std::optional<Json>()> remoteManagedSettings;
  std::function<ParsedSettings()> getMdmSettings,getHkcuSettings;
  std::function<std::optional<Json>()> flagSettingsInline;
  std::string platform;
};
struct SettingsWithSources {
  Json effective=Json::object();
  Json sources=Json::array();
};
class Loader {
 public:
  explicit Loader(LoadServices services);
  Json getSettingsForSource(Source source);
  std::optional<std::string> getPolicySettingsOrigin();
  ParsedSettings getSettingsWithErrors();
  Json getInitialSettings();
  SettingsWithSources getSettingsWithSources();
  bool rawSettingsContainsKey(const std::string& key);
 private:
  Json getSettingsForSourceUncached(Source source);
  ParsedSettings loadSettingsFromDisk();
  LoadServices services_;
  bool loading_=false;
};
}
