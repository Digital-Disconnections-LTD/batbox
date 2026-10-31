#pragma once
#include "settings/Paths.hpp"
#include "settings/Parse.hpp"
#include "settings/persistence/InternalWrites.hpp"
#include "settings/persistence/SafeJson.hpp"
#include <functional>
#include <optional>
#include <vector>

namespace batbox::settings::persistence {
struct UpdatePatch {
  Json value=Json::object();
  // An explicitly undefined property, represented outside JSON.
  std::vector<std::vector<std::string>> undefinedPaths;
};
struct UpdateServices {
  Paths* paths=nullptr;
  Stash* stash=nullptr;
  InternalWrites* internalWrites=nullptr;
  SafeJsonParser* safeJsonParser=nullptr;
  ParseServices parser;
  std::string originalCwd;
  std::function<void(const std::string&,const std::string&)> scheduleGitignore;
  std::function<void(const std::string&)> logError;
  std::function<void(const std::string&)> debug;
};
struct UpdateResult {std::optional<std::string> error;};
Json mergeForUpdate(const Json& existing,const UpdatePatch& patch);
UpdateResult updateSettingsForSource(Source source,const UpdatePatch& patch,
  const UpdateServices& services);
}
