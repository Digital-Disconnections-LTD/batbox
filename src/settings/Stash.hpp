#pragma once
#include "runtime/JsonValue.hpp"
#include "settings/Sources.hpp"
#include <map>
#include <optional>
#include <string>

namespace batbox::settings {
struct ParsedSettings {
  Json settings=nullptr;
  Json errors=Json::array();
};
class Stash {
 public:
  std::optional<Json> getFlightSettingsStash() const;
  void setFlightSettingsStash(const Json& value);
  std::optional<Json> getStashedSettingsForSource(Source source) const;
  void setStashedSettingsForSource(Source source,const Json& value);
  std::optional<ParsedSettings> getStashedParsedFile(
    const std::string& path) const;
  void setStashedParsedFile(const std::string& path,
    const ParsedSettings& value);
  void resetSettingsStash();
  std::optional<Json> getPluginSettingsBase() const;
  void setPluginSettingsBase(const std::optional<Json>& settings);
  void clearPluginSettingsBase();
 private:
  std::optional<Json> flightSettingsStash_;
  std::map<Source,Json> perSourceStash_;
  std::map<std::string,ParsedSettings> parseFileStash_;
  std::optional<Json> pluginSettingsBase_;
};
}
