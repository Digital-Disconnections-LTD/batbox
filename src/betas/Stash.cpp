#include "betas/Betas.hpp"
#include "utils/envUtils.hpp"
#include <algorithm>

namespace batbox::betas {
namespace {
bool includes(const std::vector<std::string>& values,
    const std::string& value){
  return std::find(values.begin(),values.end(),value)!=values.end();
}
}
std::shared_ptr<std::vector<std::string>>
BetaHeaders::getAllModelBetas(const std::string& model){
  if(const auto found=all_.find(model);found!=all_.end())return found->second;
  auto output=std::make_shared<std::vector<std::string>>();
  const auto canonical=services_.canonicalName(model);
  const bool isHaiku=canonical.find(services_.patterns.haiku)!=
    std::string::npos;
  const auto provider=services_.provider();
  const bool includeFirstPartyOnly=shouldIncludeFirstPartyOnlyBetas();
  const auto& header=services_.headers;
  if(!isHaiku){
    output->push_back(header.core);
    if(internalCli()&&!header.internal.empty())
      output->push_back(header.internal);
  }
  if(services_.isSubscriber())output->push_back(header.oauth);
  if(services_.has1mContext(model))
    output->push_back(header.context1m);
  if(!isEnvTruthy(env(services_.environmentKeys.disableInterleaved))&&
      modelSupportsISP(model))
    output->push_back(header.interleavedThinking);
  if(includeFirstPartyOnly&&modelSupportsISP(model)&&
      !services_.nonInteractive()&&
      !services_.showThinkingSummaries())
    output->push_back(header.redactThinking);
  if(!header.summarizeConnectorText.empty()&&
      env(services_.environmentKeys.userType)==
        services_.environmentKeys.internalUserType&&
      includeFirstPartyOnly&&
      !isEnvDefinedFalsy(env(services_.environmentKeys.summarizeConnector))&&
      (isEnvTruthy(env(services_.environmentKeys.summarizeConnector))||
        value::truthy(services_.featureValue("tengu_slate_prism",false))))
    output->push_back(header.summarizeConnectorText);
  const bool optedIntoClearing=
    isEnvTruthy(env(services_.environmentKeys.contextManagement))&&
    env(services_.environmentKeys.userType)==
      services_.environmentKeys.internalUserType;
  const bool preserveThinking=modelSupportsContextManagement(model);
  if(shouldIncludeFirstPartyOnlyBetas()&&
      (optedIntoClearing||preserveThinking))
    output->push_back(header.contextManagement);
  const bool strict=services_.featureGate("tengu_tool_pear");
  const bool efficient=!strict&&value::truthy(services_.featureValue(
    "tengu_amber_json_tools",false));
  if(includeFirstPartyOnly&&modelSupportsStructuredOutputs(model)&&strict)
    output->push_back(header.structuredOutputs);
  if(env(services_.environmentKeys.userType)==
      services_.environmentKeys.internalUserType&&
      includeFirstPartyOnly&&efficient)
    output->push_back(header.tokenEfficientGadgets);
  if(provider=="vertex"&&vertexModelSupportsWebSearch(model))
    output->push_back(header.webSearch);
  if(provider=="foundry")output->push_back(header.webSearch);
  if(includeFirstPartyOnly)
    output->push_back(header.poetryStashingScope);
  const auto extra=env(services_.environmentKeys.additionalBetas);
  if(value::truthy(extra)){
    const auto text=extra.get<std::string>();
    std::size_t start=0;
    for(;;){
      const auto end=text.find(',',start);
      const auto value=value::trim(text.substr(start,end==std::string::npos
        ?std::string::npos:end-start));
      if(!value.empty())output->push_back(value);
      if(end==std::string::npos)break;
      start=end+1;
    }
  }
  all_[model]=output;
  return output;
}
std::shared_ptr<std::vector<std::string>>
BetaHeaders::getModelBetas(const std::string& model){
  if(const auto found=model_.find(model);found!=model_.end())
    return found->second;
  auto all=getAllModelBetas(model);
  if(services_.provider()!="bedrock"){
    model_[model]=all;return all;
  }
  auto output=std::make_shared<std::vector<std::string>>();
  for(const auto& beta:*all)
    if(!includes(services_.headers.bedrockExtraBodyParams,beta))
      output->push_back(beta);
  model_[model]=output;
  return output;
}
std::shared_ptr<std::vector<std::string>>
BetaHeaders::getBedrockExtraBodyParamsBetas(const std::string& model){
  if(const auto found=bedrockExtra_.find(model);found!=bedrockExtra_.end())
    return found->second;
  auto all=getAllModelBetas(model);
  auto output=std::make_shared<std::vector<std::string>>();
  for(const auto& beta:*all)
    if(includes(services_.headers.bedrockExtraBodyParams,beta))
      output->push_back(beta);
  bedrockExtra_[model]=output;
  return output;
}
std::vector<std::string> BetaHeaders::getMergedBetas(
    const std::string& model,bool isAgenticQuery){
  auto output=*getModelBetas(model);
  if(isAgenticQuery){
    if(!includes(output,services_.headers.core))
      output.push_back(services_.headers.core);
    if(internalCli()&&!services_.headers.internal.empty()&&
        !includes(output,services_.headers.internal))
      output.push_back(services_.headers.internal);
  }
  const auto sdk=services_.sdkBetas();
  if(!sdk||sdk->empty())return output;
  const auto base=output;
  for(const auto& beta:*sdk)
    if(!includes(base,beta))
      output.push_back(beta);
  return output;
}
void BetaHeaders::clearBetasStashes(){
  all_.clear();model_.clear();bedrockExtra_.clear();
}
}
