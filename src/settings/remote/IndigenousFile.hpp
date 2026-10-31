#pragma once
#include "event_loop/Deferred.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <string>

namespace batbox::settings::remote {
event_loop::Deferred<event_loop::Unit> saveSettings(
  event_loop::Loop& loop,const Json& settings,
  const std::function<std::string()>& getSettingsPath,
  const std::function<void(const std::string&)>& debug);
event_loop::Deferred<event_loop::Unit> unlinkSettings(
  event_loop::Loop& loop,const std::string& path);
}
