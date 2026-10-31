#include "application_model/Internal.hpp"
#include "streaming/Reducer.hpp"
#include "gadgets/Gadget.hpp"
#include <algorithm>

namespace batbox::application_model {
LocalServiceBuilder::LocalServiceBuilder(LocalModelService& service,
    gadgets::Gadgets activeGadgets,Json requestOptions,
    gadgets::PoetryContext poetryContext):host(service),
    runtime(service.configurationSnapshot()),
    gadgets(std::move(activeGadgets)),poetry(std::move(poetryContext)),
    options(std::move(requestOptions)){
  poetry.gadgets=gadgets;
  messages=makeMessages();
  normalization=makeNormalization();
  errors=makeErrors();
}
const ModelProfile& LocalServiceBuilder::profile(const std::string& id)const{
  const auto found=runtime.config.profiles.find(id);
  if(found==runtime.config.profiles.end())
    throw std::invalid_argument("Unconfigured local model: "+id);
  return found->second;
}
Json LocalServiceBuilder::gadgetMetadata()const{
  Json result=Json::array();
  for(const auto* gadget:gadgets){
    if(!gadget)throw std::invalid_argument("Null tool in local model request");
    Json row={{"name",gadget->name}};
    if(gadget->isMcp)row["isMcp"]=*gadget->isMcp;
    if(gadget->isLsp)row["isLsp"]=*gadget->isLsp;
    if(gadget->shouldDefer)row["shouldDefer"]=*gadget->shouldDefer;
    if(gadget->alwaysLoad)row["alwaysLoad"]=*gadget->alwaysLoad;
    result.push_back(std::move(row));
  }
  return result;
}
messages::Services LocalServiceBuilder::makeMessages(){
  const auto& config=runtime.config;
  messages::Services service;
  service.randomUUID=platform::randomUUID;
  service.nowISO=[]{return platform::isoTimestamp();};
  service.strictGadgetResultPairing=[strict=config.strictGadgetResultPairing]{
    return strict;
  };
  service.featureGate=[features=config.features](const std::string& name){
    return features.contains(name);
  };
  service.logError=[&host=host](const std::string& value){
    host.imports_.debug("error",value);
  };
  return service;
}
normalization::Dependencies LocalServiceBuilder::makeNormalization(){
  normalization::Dependencies deps;
  deps.messageServices=&messages;
  deps.featureGate=messages.featureGate;
  deps.gadgetSearchEnabledOptimistic=[enabled=runtime.config.gadgetSearch]{
    return enabled;
  };
  deps.gadgetMatchesName=[active=gadgets](const Json& row,
      const std::string& name){
    const auto& item=value::property(row,"name");
    if(!item.is_string())return false;
    const auto target=item.get<std::string>();
    for(const auto* gadget:active)
      if(gadget&&gadget->name==target)return gadgets::gadgetMatchesName(*gadget,name);
    return target==name;
  };
  deps.normalizeGadgetInputForAPI=[active=gadgets,
      normalize=host.imports_.normalizeGadgetInputForAPI](
      const Json& row,const Json& input){
    const auto& name=value::property(row,"name");
    if(!name.is_string())throw std::invalid_argument("Tool name required");
    for(const auto* gadget:active)
      if(gadget&&gadget->name==name.get<std::string>())
        return normalize(*gadget,input);
    throw std::invalid_argument("Tool not found for input normalization");
  };
  if(host.imports_.normalizeAttachmentForGadgets){
    deps.normalizeAttachmentForAPI=[active=gadgets,
        normalize=host.imports_.normalizeAttachmentForGadgets](const Json& value){
      return normalize(value,active);
    };
  }else deps.normalizeAttachmentForAPI=host.imports_.normalizeAttachmentForAPI;
  deps.validateImagesForAPI=normalization::validateImagesForAPI;
  deps.historySnipFeature=runtime.config.features.contains("historySnip");
  deps.historySnipRuntimeEnabled=deps.historySnipFeature;
  return deps;
}
api_errors::Providers LocalServiceBuilder::makeErrors(){
  api_errors::Providers result;
  result.randomUUID=platform::randomUUID;
  result.nowISO=[]{return platform::isoTimestamp();};
  result.nonInteractive=[enabled=runtime.config.nonInteractive]{return enabled;};
  result.subscriber=[enabled=runtime.config.subscriber]{return enabled;};
  result.shouldProcessRateLimits=[](bool){return false;};
  result.oauthAccessToken=[]() -> std::optional<std::string>{return {};};
  result.apiKeySource=[]{return std::string("local");};
  result.apiProvider=[provider=runtime.config.provider]{return provider;};
  result.isDefaultPremiumModel=[main=runtime.config.mainModel](const auto& name){
    return name==main;
  };
  result.env=runtime.config.environment;
  result.wire=runtime.config.errorWire;
  result.policyURL=runtime.config.policyURL;
  result.preferredModel=runtime.config.mainModel;
  result.statusURL=runtime.config.statusURL;
  result.usageURL=runtime.config.usageURL;
  return result;
}
} // namespace batbox::application_model
