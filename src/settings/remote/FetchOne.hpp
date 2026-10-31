#pragma once
#include "settings/Parse.hpp"
#include "settings/remote/Auth.hpp"
#include "settings/remote/Http.hpp"
#include "settings/remote/ResponseSchema.hpp"

namespace batbox::settings::remote {
struct FetchOneServices {
  std::function<event_loop::Deferred<event_loop::Unit>()> refreshOAuth;
  AuthServices auth;
  std::function<std::string()> baseApiUrl,userBat;
  std::string settingsPath="/api/batbox/settings";
  std::function<event_loop::Deferred<HttpResponse>(
    const std::string&,const Json&,double)> httpGet;
  std::function<ResponseParseResult(const Json&)> validateResponse;
  std::function<SchemaResult(const Json&)> validateSettings;
  std::function<std::string(const SchemaResult&)> settingsErrorMessage;
  std::function<void(const std::string&)> debug;
  double timeoutMs=10000;
};
event_loop::Deferred<FetchResult> fetchRemoteManagedSettings(
  event_loop::Loop& loop,const std::optional<std::string>& stashedChecksum,
  const FetchOneServices& services);
}
