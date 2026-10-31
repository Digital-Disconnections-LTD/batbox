#include "bootstrap/CoreFlags.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
using batbox::Json;
Json optionalString(const std::optional<std::string>& value){
  return value?Json(*value):Json(nullptr);
}
Json optionalJson(const std::optional<Json>& value){
  return value?*value:Json(nullptr);
}
Json vectorJson(const std::shared_ptr<std::vector<std::string>>& values){
  Json output=Json::array();
  if(values)for(const auto& value:*values)output.push_back(value);
  return output;
}
}
int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    bootstrap::CoreFlags state;
    Json trace=Json::array();
    std::shared_ptr<Json> inlineSettings;
    std::shared_ptr<std::vector<std::string>> plugins,sources;
    if(test.contains("ops"))for(const auto& op:test["ops"]){
      const auto kind=op.value("kind",std::string());
      if(kind=="interactive")bootstrap::setIsInteractive(state,
        op["value"].get<bool>());
      else if(kind=="client")bootstrap::setClientType(state,
        op["value"].get<std::string>());
      else if(kind=="progress")
        bootstrap::setSdkBatProgressSummariesEnabled(state,
          op["value"].get<bool>());
      else if(kind=="kairos")bootstrap::setKairosActive(state,
        op["value"].get<bool>());
      else if(kind=="strict")bootstrap::setStrictGadgetResultPairing(state,
        op["value"].get<bool>());
      else if(kind=="optIn")bootstrap::setUserMsgOptIn(state,
        op["value"].get<bool>());
      else if(kind=="source")bootstrap::setFlightSource(state,
        op["value"].get<std::string>());
      else if(kind=="preview")bootstrap::setQuestionPreviewFormat(state,
        op["value"].get<std::string>());
      else if(kind=="color"){
        auto colors=bootstrap::getBatColorMap(state);
        const auto name=op["name"].get<std::string>();
        const auto value=op["value"].get<std::string>();
        const auto found=std::find_if(colors->begin(),colors->end(),
          [&](const auto& entry){return entry.first==name;});
        if(found==colors->end())colors->emplace_back(name,value);
        else found->second=value;
      }else if(kind=="flagPath"){
        std::optional<std::string> path;
        if(op.contains("value")&&!op["value"].is_null())
          path=op["value"].get<std::string>();
        bootstrap::setFlagSettingsPath(state,path);
      }else if(kind=="inline"){
        inlineSettings=std::make_shared<Json>(op["value"]);
        bootstrap::setFlagSettingsInline(state,inlineSettings);
      }else if(kind=="mutateInline")
        (*inlineSettings)[op["key"].get<std::string>()]=op["value"];
      else if(kind=="ingress")bootstrap::setFlightIngressToken(state,
        op["value"]);
      else if(kind=="oauthFd")bootstrap::setOauthTokenFromFd(state,
        op["value"]);
      else if(kind=="keyFd")bootstrap::setApiKeyFromFd(state,op["value"]);
      else if(kind=="sources"){
        sources=std::make_shared<std::vector<std::string>>();
        for(const auto& item:op["value"])
          sources->push_back(item.get<std::string>());
        bootstrap::setAllowedSettingSources(state,sources);
      }else if(kind=="appendSource")
        sources->push_back(op["value"].get<std::string>());
      else if(kind=="plugins"){
        plugins=std::make_shared<std::vector<std::string>>();
        for(const auto& item:op["value"])
          plugins->push_back(item.get<std::string>());
        bootstrap::setInlinePlugins(state,plugins);
      }else if(kind=="appendPlugin")
        plugins->push_back(op["value"].get<std::string>());
      else if(kind=="chrome"){
        std::optional<bool> value;
        if(op.contains("value")&&!op["value"].is_null())
          value=op["value"].get<bool>();
        bootstrap::setChromeFlagOverride(state,value);
      }else if(kind=="workspace")bootstrap::setUseRoostPlugins(state,
        op["value"].get<bool>(),[&]{trace.push_back("resetCache");});
      else if(kind=="bypass")
        bootstrap::setFlightBypassPermissionsMode(state,
          op["value"].get<bool>());
      else if(kind=="scheduled")
        bootstrap::setScheduledMissionsEnabled(state,op["value"].get<bool>());
      else throw std::runtime_error("Unknown flag operation");
    }
    Json colors=Json::array();
    for(const auto& [name,value]:*bootstrap::getBatColorMap(state))
      colors.push_back(Json::array({name,value}));
    const auto activeInline=bootstrap::getFlagSettingsInline(state);
    const auto activeChrome=bootstrap::getChromeFlagOverride(state);
    const Json result={{"interactive",bootstrap::getIsInteractive(state)},
      {"nonInteractive",bootstrap::getIsNonInteractiveFlight(state)},
      {"clientType",bootstrap::getClientType(state)},
      {"thirdParty",bootstrap::preferThirdPartyAuthentication(
        state,"batbox-vscode")},
      {"progress",bootstrap::getSdkBatProgressSummariesEnabled(state)},
      {"kairos",bootstrap::getKairosActive(state)},
      {"strict",bootstrap::getStrictGadgetResultPairing(state)},
      {"optIn",bootstrap::getUserMsgOptIn(state)},
      {"source",optionalString(bootstrap::getFlightSource(state))},
      {"preview",optionalString(bootstrap::getQuestionPreviewFormat(state))},
      {"colors",colors},
      {"flagPath",optionalString(bootstrap::getFlagSettingsPath(state))},
      {"inline",activeInline?*activeInline:Json(nullptr)},
      {"sameInline",inlineSettings?Json(activeInline==inlineSettings):
        Json(nullptr)},
      {"ingress",optionalJson(bootstrap::getFlightIngressToken(state))},
      {"ingressSet",bootstrap::getFlightIngressToken(state).has_value()},
      {"oauthFd",optionalJson(bootstrap::getOauthTokenFromFd(state))},
      {"oauthFdSet",bootstrap::getOauthTokenFromFd(state).has_value()},
      {"keyFd",optionalJson(bootstrap::getApiKeyFromFd(state))},
      {"keyFdSet",bootstrap::getApiKeyFromFd(state).has_value()},
      {"sources",vectorJson(bootstrap::getAllowedSettingSources(state))},
      {"sameSources",sources?
        Json(bootstrap::getAllowedSettingSources(state)==sources):
        Json(nullptr)},
      {"plugins",vectorJson(bootstrap::getInlinePlugins(state))},
      {"samePlugins",plugins?Json(bootstrap::getInlinePlugins(state)==
        plugins):Json(nullptr)},
      {"chrome",activeChrome?Json(*activeChrome):Json(nullptr)},
      {"chromeSet",activeChrome.has_value()},
      {"workspace",bootstrap::getUseRoostPlugins(state)},
      {"bypass",bootstrap::getFlightBypassPermissionsMode(state)},
      {"scheduled",bootstrap::getScheduledMissionsEnabled(state)},
      {"trace",trace}};
    std::cout<<value::stringify(result)<<'\n';
  }
}
