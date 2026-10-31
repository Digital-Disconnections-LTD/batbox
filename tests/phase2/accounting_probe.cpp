#include "accounting/Accounting.hpp"
#include <iostream>
using batbox::Json;
using namespace batbox::accounting;
static Json evaluate(const Json& input){
 AccountingState state;state.modelUsage=input.value("initialModelUsage",Json::object());
 state.totalCostUSD=input.value("initialTotalCostUSD",0.0);
 CostServices services;services.modelCosts=input.value("modelCosts",Json::object());
 services.premiumCurrentStandardCosts=input.at("premiumStandardCosts");
 services.premiumCurrentFastCosts=input.at("premiumFastCosts");
 services.unknownFallbackCosts=input.at("unknownFallbackCosts");
 services.premiumCurrentCanonical="premium-current";
 services.getCanonicalName=[&](const std::string& m){auto map=input.value("canonicalMap",Json::object());
   return map.is_object()&&map.contains(m)?map.at(m).get<std::string>():m;};
 services.getDefaultMainLoopModelSetting=[&]{return input.value("defaultModel",std::string("premium-current"));};
 services.isFastModeEnabled=[&]{return input.value("fastModeEnabled",false);};
 services.getSdkBetas=[&]{return input.value("betas",Json::array());};
 services.getContextWindowForModel=[&](const std::string& model,const Json&){
   auto windows=input.value("contextWindows",Json::object());
   return windows.is_object()&&windows.contains(model)?windows.at(model).get<double>():200000.0;};
 services.getModelMaxOutputTokens=[&](const std::string& model){
   auto limits=input.value("maxOutputs",Json::object());
   return Json{{"default",limits.is_object()&&limits.contains(model)?limits.at(model):Json(32000)}};};
 const auto op=input.at("op").get<std::string>();Json result;
 if(op=="costs")result=getModelCosts(input.at("model").get<std::string>(),input.at("usage"),state,services);
 else if(op=="calculate")result=calculateUSDCost(input.at("model").get<std::string>(),input.at("usage"),state,services);
 else if(op=="tokens")result=tokensToUSDCost(input.at("costs"),input.at("usage"));
 else if(op=="fromTokens")result=calculateCostFromTokens(input.at("model").get<std::string>(),input.at("tokens"),state,services);
 else if(op=="advisors")result=getAdvisorUsage(input.at("usage"));
 else if(op=="modelUsage")result=addToTotalModelUsage(input.at("cost").get<double>(),input.at("usage"),input.at("model").get<std::string>(),state,services);
 else if(op=="session")result=addToTotalFlightCost(input.at("cost").get<double>(),input.at("usage"),input.at("model").get<std::string>(),state,services);
 else throw std::logic_error("unknown op");
 return {{"result",result},{"state",{{"modelUsage",state.modelUsage},{"totalCostUSD",state.totalCostUSD},{"hasUnknownModelCost",state.hasUnknownModelCost}}}};
}
int main(){std::string line;while(std::getline(std::cin,line))try{std::cout<<evaluate(Json::parse(line)).dump()<<'\n';}
 catch(const std::exception& e){std::cout<<Json{{"fatal",e.what()}}.dump()<<'\n';}}
