#include "bootstrap/CoreFlags.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::bootstrap {
bool getIsNonInteractiveFlight(const CoreFlags& s){return !s.isInteractive;}
bool getIsInteractive(const CoreFlags& s){return s.isInteractive;}
void setIsInteractive(CoreFlags& s,bool value){s.isInteractive=value;}
const std::string& getClientType(const CoreFlags& s){return s.clientType;}
void setClientType(CoreFlags& s,std::string type){s.clientType=std::move(type);}
bool getSdkBatProgressSummariesEnabled(const CoreFlags& s){
  return s.sdkBatProgressSummariesEnabled;}
void setSdkBatProgressSummariesEnabled(CoreFlags& s,bool value){
  s.sdkBatProgressSummariesEnabled=value;}
bool getKairosActive(const CoreFlags& s){return s.kairosActive;}
void setKairosActive(CoreFlags& s,bool value){s.kairosActive=value;}
bool getStrictGadgetResultPairing(const CoreFlags& s){
  return s.strictGadgetResultPairing;}
void setStrictGadgetResultPairing(CoreFlags& s,bool value){
  s.strictGadgetResultPairing=value;}
bool getUserMsgOptIn(const CoreFlags& s){return s.userMsgOptIn;}
void setUserMsgOptIn(CoreFlags& s,bool value){s.userMsgOptIn=value;}
const std::optional<std::string>& getFlightSource(const CoreFlags& s){
  return s.flightSource;}
void setFlightSource(CoreFlags& s,std::string source){
  s.flightSource=std::move(source);}
const std::optional<std::string>& getQuestionPreviewFormat(
    const CoreFlags& s){return s.questionPreviewFormat;}
void setQuestionPreviewFormat(CoreFlags& s,std::string format){
  s.questionPreviewFormat=std::move(format);}
std::shared_ptr<std::vector<std::pair<std::string,std::string>>>
    getBatColorMap(const CoreFlags& s){return s.batColorMap;}
const std::optional<std::string>& getFlagSettingsPath(const CoreFlags& s){
  return s.flagSettingsPath;}
void setFlagSettingsPath(CoreFlags& s,std::optional<std::string> path){
  s.flagSettingsPath=std::move(path);}
std::shared_ptr<Json> getFlagSettingsInline(const CoreFlags& s){
  return s.flagSettingsInline;}
void setFlagSettingsInline(CoreFlags& s,std::shared_ptr<Json> settings){
  s.flagSettingsInline=std::move(settings);}
const std::optional<Json>& getFlightIngressToken(const CoreFlags& s){
  return s.flightIngressToken;}
void setFlightIngressToken(CoreFlags& s,Json token){
  s.flightIngressToken=std::move(token);}
const std::optional<Json>& getOauthTokenFromFd(const CoreFlags& s){
  return s.oauthTokenFromFd;}
void setOauthTokenFromFd(CoreFlags& s,Json token){
  s.oauthTokenFromFd=std::move(token);}
const std::optional<Json>& getApiKeyFromFd(const CoreFlags& s){
  return s.apiKeyFromFd;}
void setApiKeyFromFd(CoreFlags& s,Json key){s.apiKeyFromFd=std::move(key);}
std::shared_ptr<std::vector<std::string>> getAllowedSettingSources(
    const CoreFlags& s){return s.allowedSettingSources;}
void setAllowedSettingSources(CoreFlags& s,
    std::shared_ptr<std::vector<std::string>> sources){
  if(!sources)throw std::invalid_argument("Allowed settings sources required");
  s.allowedSettingSources=std::move(sources);}
bool preferThirdPartyAuthentication(const CoreFlags& s,
    const std::string& ideClientType){
  return getIsNonInteractiveFlight(s)&&s.clientType!=ideClientType;
}
std::shared_ptr<std::vector<std::string>> getInlinePlugins(
    const CoreFlags& s){return s.inlinePlugins;}
void setInlinePlugins(CoreFlags& s,
    std::shared_ptr<std::vector<std::string>> plugins){
  if(!plugins)throw std::invalid_argument("Inline plugin list required");
  s.inlinePlugins=std::move(plugins);}
const std::optional<bool>& getChromeFlagOverride(const CoreFlags& s){
  return s.chromeFlagOverride;}
void setChromeFlagOverride(CoreFlags& s,std::optional<bool> value){
  s.chromeFlagOverride=value;}
bool getUseRoostPlugins(const CoreFlags& s){
  return s.useRoostPlugins;}
void setUseRoostPlugins(CoreFlags& s,bool value,
    const std::function<void()>& resetSettingsStash){
  if(!resetSettingsStash)
    throw std::invalid_argument("Missing settings cache reset binding");
  s.useRoostPlugins=value;
  resetSettingsStash();
}
bool getFlightBypassPermissionsMode(const CoreFlags& s){
  return s.flightBypassPermissionsMode;}
void setFlightBypassPermissionsMode(CoreFlags& s,bool enabled){
  s.flightBypassPermissionsMode=enabled;}
bool getScheduledMissionsEnabled(const CoreFlags& s){
  return s.scheduledMissionsEnabled;}
void setScheduledMissionsEnabled(CoreFlags& s,bool enabled){
  s.scheduledMissionsEnabled=enabled;}
}
