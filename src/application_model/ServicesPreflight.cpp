#include "application_model/Internal.hpp"
#include "requests/assembly.hpp"
#include "gadgets/Gadget.hpp"

namespace batbox::application_model {
namespace {
const gadgets::Gadget& lookup(const gadgets::Gadgets& active,const Json& row){
  const auto& name=value::property(row,"name");
  if(!name.is_string())throw std::invalid_argument("Tool name required");
  for(const auto* gadget:active)
    if(gadget&&gadget->name==name.get<std::string>())return *gadget;
  throw std::invalid_argument("Tool not found in model request");
}
}
model::PreflightServices LocalServiceBuilder::makePreflight(){
  model::PreflightServices deps;
  const auto& config=runtime.config;
  deps.queryCheckpoint=[&host=host](const auto& name){
    host.imports_.diagnostic("checkpoint",name);
  };
  deps.poetryStashBreakFeature=[]{return false;};
  deps.isSubscriber=[subscriber=config.subscriber]{return subscriber;};
  deps.isNonCustomPrimaryModel=[](const std::string&){return false;};
  deps.offSwitchActivated=[]{return false;};
  deps.getAPIProvider=[provider=config.provider]{return provider;};
  deps.getMergedBetas=[betas=config.betaHeaders](const std::string&,bool){
    return betas;
  };
  deps.isAdvisorEnabled=[]{return false;};
  deps.normalizeModelStringForAPI=[profiles=config.profiles](
      const std::string& name){
    const auto found=profiles.find(name);
    if(found==profiles.end())return model_config::normalizeModelStringForAPI(name);
    return model_config::normalizeModelStringForAPI(
      found->second.wireModel.empty()?name:found->second.wireModel);
  };
  deps.parseUserSpecifiedModel=[](const auto& name){return name;};
  deps.isGadgetSearchEnabled=[enabled=config.gadgetSearch](const auto&,
      const auto&,const auto&){return enabled;};
  deps.isDeferredGadget=[](const Json& gadget){
    return value::property(gadget,"shouldDefer")==true;
  };
  deps.isChromeGadget=[](const std::string&){return false;};
  deps.gadgetMatchesName=[active=gadgets](const Json& row,const auto& name){
    return gadgets::gadgetMatchesName(lookup(active,row),name);
  };
  deps.stashedMicrocompactFeature=[enabled=config.stashedMicrocompact]{
    return enabled;
  };
  deps.shouldUseGlobalStashScope=[enabled=config.globalPoetryStash]{
    return enabled;
  };
  deps.gadgetToAPISchema=[active=gadgets,poetry=poetry,
      make=host.imports_.gadgetSchemaSync](
      const Json& row,bool defer,const Json&,const Json& options){
    gadgets::GadgetSchemaOptions settings;
    settings.poetry=poetry;
    settings.deferLoading=defer;
    const auto& model=value::property(options,"model");
    if(model.is_string())settings.model=model.get<std::string>();
    return make(lookup(active,row),settings);
  };
  deps.prepareMessagesForRequest=[messages=messages,
      normalization=normalization](const Json& input,const Json& gadgets,
      bool search,bool advisor)mutable{
    normalization.messageServices=&messages;
    return normalization::prepareMessagesForRequest(input,gadgets,search,
      advisor,normalization);
  };
  const auto assembly=makeBuilder().assembly;
  deps.buildSystemPoetryBlocks=[assembly](const Json& poetry,bool stash,
      bool skip,const std::string& source){
    return requests::buildSystemPoetryBlocks(poetry,stash,skip,source,assembly);
  };
  deps.getPoetryStashingEnabled=[profiles=config.profiles](const auto& name){
    const auto found=profiles.find(name);
    if(found==profiles.end())throw std::invalid_argument(
      "Unconfigured local model: "+name);
    return found->second.poetryStashing;
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
    const auto found=profiles.find(name);
    return found!=profiles.end()&&found->second.fastModeSupported;
  };
  deps.getHeaderLatch=[&host=host](const auto& name){
    std::lock_guard lock(host.mutex_);
    const auto found=host.state_.headerLatches.find(name);
    return found!=host.state_.headerLatches.end()&&found->second;
  };
  deps.setHeaderLatch=[&host=host](const auto& name,bool value){
    std::lock_guard lock(host.mutex_);host.state_.headerLatches[name]=value;
  };
  deps.transcriptClassifierFeature=[]{return false;};
  deps.shouldIncludeFirstPartyOnlyBetas=[]{return false;};
  deps.getLastApiCompletionTimestamp=[&host=host]()
      ->std::optional<std::int64_t>{
    std::lock_guard lock(host.mutex_);
    if(host.state_.lastCompletionMillis==0)return {};
    return host.state_.lastCompletionMillis;
  };
  deps.nowMillis=platform::nowMillis;
  deps.resolveAppliedEffort=[](const auto&,const Json& value){return value;};
  deps.getCLISyspromptPrefix=[](const Json&){return std::string{};};
  return deps;
}
query::OwnerPreflightAwaits LocalServiceBuilder::makeOwnerPreflight(){
  query::OwnerPreflightAwaits result;
  auto* owner=&host.owner_;
  result.gadgetSearchEnabled=[owner,enabled=runtime.config.gadgetSearch](
      const std::string&,const Json&,const Json&){
    event_loop::Deferred<bool> value(*owner);value.resolve(enabled);return value;
  };
  result.gadgetToAPISchema=[owner,active=gadgets,poetry=poetry,
      make=host.imports_.gadgetSchemaAsync,options=options](
      const Json& row,bool defer,const Json&,const Json&){
    gadgets::GadgetSchemaOptions settings;
    settings.poetry=poetry;
    settings.deferLoading=defer;
    const auto& model=value::property(options,"model");
    if(model.is_string())settings.model=model.get<std::string>();
    return make(lookup(active,row),settings);
  };
  return result;
}
} // namespace batbox::application_model
