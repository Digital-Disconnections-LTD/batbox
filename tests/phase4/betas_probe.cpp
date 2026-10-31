#include "betas/Betas.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cctype>
#include <iostream>

int main(){
  using namespace batbox;
  std::string line;
  while(std::getline(std::cin,line))if(!line.empty()){
    const auto test=value::parse(line);
    Json warnings=Json::array();
    int canonicalCalls=0,providerCalls=0;
    betas::Services services;
    services.environmentKeys.internalUserType="batbox";
    services.environment=[&](const std::string& key)->Json{
      const auto& names=services.environmentKeys;
      const char* sourceKey=nullptr;
      if(key==names.userType)sourceKey="userType";
      else if(key==names.entrypoint)sourceKey="entrypoint";
      else if(key==names.disableExperimental)sourceKey=
        "disableExperimental";
      else if(key==names.disableInterleaved)sourceKey="disableInterleaved";
      else if(key==names.summarizeConnector)sourceKey="summarizeConnector";
      else if(key==names.contextManagement)sourceKey="contextManagement";
      else if(key==names.additionalBetas)sourceKey="additionalBetas";
      if(!sourceKey||!test.contains(sourceKey))return nullptr;
      if(std::string(sourceKey)=="userType"&&
          test[sourceKey]=="internal")return "batbox";
      return test[sourceKey];
    };
    services.canonicalName=[&](const std::string& model){
      ++canonicalCalls;
      auto result=model;
      std::transform(result.begin(),result.end(),result.begin(),
        [](unsigned char value){return static_cast<char>(std::tolower(value));});
      return result;
    };
    services.provider=[&](){++providerCalls;
      return test.value("provider",std::string("firstParty"));};
    services.capabilityOverride=[&](const std::string&,
        const std::string&)->std::optional<bool>{
      if(!test.contains("override")||test["override"].is_null())
        return std::nullopt;
      return test["override"].get<bool>();
    };
    services.isSubscriber=[&](){return test.value("subscriber",false);};
    services.nonInteractive=[&](){return test.value("nonInteractive",false);};
    services.showThinkingSummaries=[&](){return test.value(
      "showThinkingSummaries",false);};
    services.has1mContext=[&](const std::string&){return test.value(
      "context1m",false);};
    services.featureGate=[&](const std::string&){return test.value(
      "strict",false);};
    services.buildFeature=[&](const std::string& name){
      return name=="TRANSCRIPT_CLASSIFIER"&&test.value(
        "transcriptClassifier",false);};
    services.featureValue=[&](const std::string& name,
        const Json& otherwise){
      if(name=="tengu_auto_mode_config")return test.value(
        "autoConfig",otherwise);
      if(name=="tengu_slate_prism")return test.value(
        "summaryGate",otherwise);
      if(name=="tengu_amber_json_tools")return test.value(
        "efficient",otherwise);
      return otherwise;
    };
    services.sdkBetas=[&]()->std::optional<std::vector<std::string>>{
      if(!test.contains("sdkBetas")||test["sdkBetas"].is_null())
        return std::nullopt;
      return test["sdkBetas"].get<std::vector<std::string>>();
    };
    services.warn=[&](const std::string& value){warnings.push_back(value);};
    services.headers={"H_core",
      test.value("userType",std::string())=="internal"?"H_internal":"",
      "H_oauth","H_1m","H_interleaved","H_redact",
      test.value("connectorHeader",true)?"H_summary":"",
      "H_context","H_structured","H_efficient","H_web","H_scope",
      "H_search1","H_search3",
      {"H_interleaved","H_1m","H_search3"}};
    services.patterns={"claude-3-","claude-opus-4","claude-sonnet-4",
      "claude-haiku-4","haiku",
      "claude-(opus|sonnet|haiku)-4(?!-[6-9])",
      "^claude-(opus|sonnet)-4-6",
      {"claude-sonnet-4-6","claude-sonnet-4-5",
        "claude-opus-4-1","claude-opus-4-5","claude-opus-4-6",
        "claude-haiku-4-5"}};
    betas::BetaHeaders value(services);
    const auto model=test.value("model",
      std::string("claude-sonnet-4-6"));
    Json result;
    try{
      const auto op=test.value("op",std::string());
      if(op=="filter"){
        const auto filtered=value.filterAllowedSdkBetas(
          services.sdkBetas());
        result=filtered?Json(*filtered):Json(nullptr);
      }else if(op=="model")result=Json{
        {"isp",value.modelSupportsISP(model)},
        {"web",value.vertexModelSupportsWebSearch(model)},
        {"context",value.modelSupportsContextManagement(model)},
        {"structured",value.modelSupportsStructuredOutputs(model)},
        {"auto",value.modelSupportsAutoMode(model)}};
      else if(op=="headers")result=Json{
        {"search",value.getGadgetSearchBetaHeader()},
        {"first",value.shouldIncludeFirstPartyOnlyBetas()},
        {"global",value.shouldUseGlobalStashScope()}};
      else if(op=="cache"){
        const auto all=value.getAllModelBetas(model),
          selected=value.getModelBetas(model),
          extra=value.getBedrockExtraBodyParamsBetas(model);
        const Json identity={{"allAgain",
            all==value.getAllModelBetas(model)},
          {"modelAgain",selected==value.getModelBetas(model)},
          {"extraAgain",extra==
            value.getBedrockExtraBodyParamsBetas(model)},
          {"allModel",all==selected}};
        value.clearBetasStashes();
        result=Json{{"identity",identity},
          {"afterClear",all!=value.getAllModelBetas(model)},
          {"all",*all},{"model",*selected},{"extra",*extra}};
      }else result=Json{{"all",*value.getAllModelBetas(model)},
        {"model",*value.getModelBetas(model)},
        {"extra",*value.getBedrockExtraBodyParamsBetas(model)},
        {"merged",value.getMergedBetas(model,test.value(
          "agentic",false))}};
    }catch(const std::exception& error){result=Json{{"error",error.what()}};}
    std::cout<<value::stringify(Json{{"value",result},
      {"warnings",warnings},{"canonicalCalls",canonicalCalls},
      {"providerCalls",providerCalls}})<<'\n';
  }
}
