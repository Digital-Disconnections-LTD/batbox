#include "application_model/Internal.hpp"
#include "runtime/Platform.hpp"
#include "gadgets/Gadget.hpp"

namespace batbox::application_model {
namespace {
accounting::CostServices costServices(const Config& config){
  accounting::CostServices costs;
  costs.modelCosts=Json::object();
  for(const auto& [id,profile]:config.profiles)
    costs.modelCosts[id]=profile.costs;
  costs.premiumCurrentCanonical=config.mainModel;
  costs.premiumCurrentStandardCosts=config.profiles.at(config.mainModel).costs;
  costs.premiumCurrentFastCosts=config.profiles.at(config.mainModel).fastCosts;
  costs.unknownFallbackCosts=config.profiles.at(config.mainModel).costs;
  costs.getCanonicalName=[](const auto& model){return model;};
  costs.getDefaultMainLoopModelSetting=[model=config.mainModel]{return model;};
  costs.isFastModeEnabled=[enabled=config.fastMode]{return enabled;};
  costs.getSdkBetas=[betas=config.betaHeaders]{return betas;};
  costs.getContextWindowForModel=[profiles=config.profiles](const auto& model,
      const Json&){
    const auto found=profiles.find(model);
    if(found==profiles.end())throw std::invalid_argument(
      "Unconfigured local model: "+model);
    return found->second.contextTokens;
  };
  costs.getModelMaxOutputTokens=[profiles=config.profiles](const auto& model){
    const auto found=profiles.find(model);
    if(found==profiles.end())throw std::invalid_argument(
      "Unconfigured local model: "+model);
    return Json{{"default",found->second.maxOutputTokens},
      {"upperLimit",found->second.maxOutputTokens}};
  };
  return costs;
}
}
streaming::ReducerConfig LocalServiceBuilder::makeReducer(){
  streaming::ReducerConfig deps;
  deps.nowMillis=platform::nowMillis;
  deps.randomUUID=platform::randomUUID;
  deps.timestamp=[]{return platform::isoTimestamp();};
  deps.connectorTextEnabled=runtime.config.connectorText;
  deps.normalizeContent=[active=gadgets,
      normalize=host.imports_.normalizeGadgetInputForAPI,
      requestOptions=options](const Json& blocks){
    streaming::NormalizeGadgets deps;
    deps.findGadgetByName=[active](const std::string& name)
        ->std::optional<Json>{
      for(const auto* gadget:active)
        if(gadget&&gadgets::gadgetMatchesName(*gadget,name))
          return Json{{"name",gadget->name}};
      return {};
    };
    deps.normalizeGadgetInput=[active,normalize](const Json& row,
        const Json& input,const std::string&){
      const auto& name=value::property(row,"name");
      for(const auto* gadget:active)
        if(gadget&&name==gadget->name)return normalize(*gadget,input);
      throw std::invalid_argument("Unknown tool in API content");
    };
    const auto& bat=value::property(requestOptions,"agentId");
    if(bat.is_string())deps.batId=bat.get<std::string>();
    return streaming::normalizeContentFromAPI(blocks,deps);
  };
  const auto costs=costServices(runtime.config);
  deps.calculateUSDCost=[&host=host,costs](const auto& model,
      const Json& usage){
    std::lock_guard lock(host.mutex_);
    return accounting::calculateUSDCost(model,usage,
      host.state_.accounting,costs);
  };
  deps.addToTotalFlightCost=[&host=host,costs](double cost,
      const Json& usage,const auto& model){
    std::lock_guard lock(host.mutex_);
    return accounting::addToTotalFlightCost(cost,usage,model,
      host.state_.accounting,costs);
  };
  deps.getErrorMessageIfRefusal=[errors=errors,messages=messages](
      const Json& reason,const std::string& model)mutable
      ->std::optional<Json>{
    errors.messageServices=&messages;
    std::optional<std::string> stop;
    if(reason.is_string())stop=reason.get<std::string>();
    return api_errors::getErrorMessageIfRefusal(stop,model,errors);
  };
  deps.createAssistantAPIErrorMessage=[messages=messages](
      const auto& text,const auto& kind,const auto& details)mutable{
    return messages::createAssistantAPIErrorMessage(
      Json{{"content",text},{"error",kind},{"errorDetails",details}},
      messages);
  };
  return deps;
}
} // namespace batbox::application_model
