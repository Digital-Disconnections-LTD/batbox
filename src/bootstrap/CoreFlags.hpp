#pragma once
#include "runtime/JsonValue.hpp"
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace batbox::bootstrap {
struct CoreFlags {
  bool isInteractive=false,sdkBatProgressSummariesEnabled=false,
    kairosActive=false,strictGadgetResultPairing=false,
    userMsgOptIn=false,useRoostPlugins=false,
    flightBypassPermissionsMode=false,scheduledMissionsEnabled=false;
  std::string clientType="cli";
  std::optional<std::string> flightSource,questionPreviewFormat,
    flagSettingsPath;
  std::shared_ptr<std::vector<std::pair<std::string,std::string>>>
    batColorMap=std::make_shared<
      std::vector<std::pair<std::string,std::string>>>();
  std::shared_ptr<Json> flagSettingsInline;
  std::optional<Json> flightIngressToken,oauthTokenFromFd,apiKeyFromFd;
  std::shared_ptr<std::vector<std::string>> allowedSettingSources=
    std::make_shared<std::vector<std::string>>(
      std::initializer_list<std::string>{"userSettings","projectSettings",
        "localSettings","flagSettings","policySettings"});
  std::shared_ptr<std::vector<std::string>> inlinePlugins=
    std::make_shared<std::vector<std::string>>();
  std::optional<bool> chromeFlagOverride;
};
bool getIsNonInteractiveFlight(const CoreFlags& state);
bool getIsInteractive(const CoreFlags& state);
void setIsInteractive(CoreFlags& state,bool value);
const std::string& getClientType(const CoreFlags& state);
void setClientType(CoreFlags& state,std::string type);
bool getSdkBatProgressSummariesEnabled(const CoreFlags& state);
void setSdkBatProgressSummariesEnabled(CoreFlags& state,bool value);
bool getKairosActive(const CoreFlags& state);
void setKairosActive(CoreFlags& state,bool value);
bool getStrictGadgetResultPairing(const CoreFlags& state);
void setStrictGadgetResultPairing(CoreFlags& state,bool value);
bool getUserMsgOptIn(const CoreFlags& state);
void setUserMsgOptIn(CoreFlags& state,bool value);
const std::optional<std::string>& getFlightSource(const CoreFlags& state);
void setFlightSource(CoreFlags& state,std::string source);
const std::optional<std::string>& getQuestionPreviewFormat(
  const CoreFlags& state);
void setQuestionPreviewFormat(CoreFlags& state,std::string format);
std::shared_ptr<std::vector<std::pair<std::string,std::string>>>
  getBatColorMap(const CoreFlags& state);
const std::optional<std::string>& getFlagSettingsPath(
  const CoreFlags& state);
void setFlagSettingsPath(CoreFlags& state,std::optional<std::string> path);
std::shared_ptr<Json> getFlagSettingsInline(const CoreFlags& state);
void setFlagSettingsInline(CoreFlags& state,std::shared_ptr<Json> settings);
const std::optional<Json>& getFlightIngressToken(const CoreFlags& state);
void setFlightIngressToken(CoreFlags& state,Json token);
const std::optional<Json>& getOauthTokenFromFd(const CoreFlags& state);
void setOauthTokenFromFd(CoreFlags& state,Json token);
const std::optional<Json>& getApiKeyFromFd(const CoreFlags& state);
void setApiKeyFromFd(CoreFlags& state,Json key);
std::shared_ptr<std::vector<std::string>> getAllowedSettingSources(
  const CoreFlags& state);
void setAllowedSettingSources(CoreFlags& state,
  std::shared_ptr<std::vector<std::string>> sources);
bool preferThirdPartyAuthentication(const CoreFlags& state,
  const std::string& ideClientType);
std::shared_ptr<std::vector<std::string>> getInlinePlugins(
  const CoreFlags& state);
void setInlinePlugins(CoreFlags& state,
  std::shared_ptr<std::vector<std::string>> plugins);
const std::optional<bool>& getChromeFlagOverride(const CoreFlags& state);
void setChromeFlagOverride(CoreFlags& state,std::optional<bool> value);
bool getUseRoostPlugins(const CoreFlags& state);
void setUseRoostPlugins(CoreFlags& state,bool value,
  const std::function<void()>& resetSettingsStash);
bool getFlightBypassPermissionsMode(const CoreFlags& state);
void setFlightBypassPermissionsMode(CoreFlags& state,bool enabled);
bool getScheduledMissionsEnabled(const CoreFlags& state);
void setScheduledMissionsEnabled(CoreFlags& state,bool enabled);
}
