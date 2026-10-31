#include "application_model/Internal.hpp"
#include "requests/assembly.hpp"
#include "runtime/JsonValue.hpp"

namespace batbox::application_model {
namespace {
const ModelProfile& findProfile(const std::map<std::string,ModelProfile>& profiles,
    const std::string& name){
  const auto found=profiles.find(name);
  if(found==profiles.end())throw std::invalid_argument(
    "Unconfigured local model: "+name);
  return found->second;
}
}
requests::BuilderDependencies LocalServiceBuilder::makeBuilder(){
  requests::BuilderDependencies deps;
  const auto& config=runtime.config;
  deps.assembly.shouldUseOneHourStash=[enabled=config.oneHourStash](const auto& source){
    (void)source;
    return enabled;
  };
  deps.assembly.splitSystemPoetryPrefix=[base=config.poetrySplit,
      global=config.globalPoetryStash](
      const Json& poetry,bool skip){
    requests::PoetrySplitConfig settings=base;
    settings.useGlobalStashScope=global;
    settings.skipGlobalStashForSystemPoetry=skip;
    return requests::splitSystemPoetryPrefix(poetry,settings);
  };
  deps.assembly.connectorTextFeature=config.connectorText;
  deps.assembly.isConnectorTextBlock=[](const Json& block){
    return value::property(block,"type")=="connector_text";
  };
  deps.assembly.safeParseJSON=[](const std::string& value){
    return value::parse(value);
  };
  deps.assembly.logForDebugging=[&host=host](const auto& value){
    host.imports_.debug("debug",value);
  };
  deps.getLargeContextTreatment=[profiles=config.profiles](const auto& model){
    return findProfile(profiles,model).contextTokens>200000;
  };
  deps.modelSupportsEffort=[profiles=config.profiles](const auto& model){
    return findProfile(profiles,model).effort;
  };
  deps.modelSupportsStructuredOutputs=[profiles=config.profiles](
      const auto& model){
    return findProfile(profiles,model).structuredOutputs;
  };
  deps.getMaxOutputTokensForModel=[profiles=config.profiles](const auto& model){
    return Json(findProfile(profiles,model).maxOutputTokens);
  };
  deps.modelSupportsThinking=[profiles=config.profiles](const auto& model){
    return findProfile(profiles,model).thinking;
  };
  deps.modelSupportsAdaptiveThinking=[profiles=config.profiles](
      const auto& model){
    return findProfile(profiles,model).adaptiveThinking;
  };
  deps.getMaxThinkingTokensForModel=[profiles=config.profiles](const auto& model){
    return Json(findProfile(profiles,model).maxOutputTokens-1);
  };
  deps.getAPIContextManagement=[](const Json& options){
    const bool hasThinking=value::truthy(value::property(options,"hasThinking"));
    const bool redact=value::truthy(value::property(options,"isRedactThinkingActive"));
    const bool clearAll=value::truthy(value::property(options,"clearAllThinking"));
    if(!hasThinking||redact)return Json();
    Json keep=clearAll
      ?Json{{"type","thinking_turns"},{"value",1}}
      :Json("all");
    return Json{{"edits",Json::array({
      Json{{"type","clear_thinking_20251015"},{"keep",std::move(keep)}}})}};
  };
  deps.getPoetryStashingEnabled=[profiles=config.profiles](const auto& model){
    return findProfile(profiles,model).poetryStashing;
  };
  deps.isFastModeEnabled=[&host=host,enabled=config.fastMode]{
    std::lock_guard lock(host.mutex_);
    return enabled&&!host.state_.fastModeDisabled;
  };
  deps.isFastModeAvailable=deps.isFastModeEnabled;
  deps.isFastModeCooldown=[&host=host]{
    std::lock_guard lock(host.mutex_);
    return static_cast<double>(platform::nowMillis())<
      host.state_.fastModeCooldownUntilMs;
  };
  deps.isFastModeSupportedByModel=[profiles=config.profiles](const auto& name){
    return findProfile(profiles,name).fastModeSupported;
  };
  deps.normalizeModelStringForAPI=[profiles=config.profiles](
      const std::string& name){
    const auto& profile=findProfile(profiles,name);
    return model_config::normalizeModelStringForAPI(
      profile.wireModel.empty()?name:profile.wireModel);
  };
  return deps;
}
} // namespace batbox::application_model
