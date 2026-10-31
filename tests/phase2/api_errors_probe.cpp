#include "api_errors/ApiErrors.hpp"
#include "retry/Retry.hpp"
#include <iostream>
#include <optional>
#include <sstream>
using batbox::Json;
using namespace batbox::api_errors;
namespace {
Json evaluate(const Json& input){
  Json trace=Json::array();std::size_t uuidIndex=0;
  batbox::messages::Services messages;
  messages.randomUUID=[&]{trace.push_back(Json{{"type","uuid"}});std::ostringstream out;out<<"00000000-0000-4000-8000-"<<std::hex;
    out.width(12);out.fill('0');out<<++uuidIndex;return out.str();};
  messages.nowISO=[&]{trace.push_back(Json{{"type","time"}});return "2026-03-31T12:34:56.000Z";};
  Providers p;p.messageServices=&messages;p.randomUUID=messages.randomUUID;p.nowISO=messages.nowISO;
  p.nonInteractive=[&]{trace.push_back(Json{{"type","nonInteractive"}});return input.value("nonInteractive",false);};
  p.subscriber=[&]{trace.push_back(Json{{"type","subscriber"}});return input.value("subscriber",false);};
  p.shouldProcessRateLimits=[&](bool sub){trace.push_back(Json{{"type","shouldProcessRateLimits"},{"subscriber",sub}});
    return input.value("processRateLimits",true);};
  p.oauthAccessToken=[&]() -> std::optional<std::string>{trace.push_back(Json{{"type","oauthAccessToken"}});
    if(input.contains("accessToken"))return input.at("accessToken").get<std::string>();
    return std::nullopt;};
  p.apiKeySource=[&]{trace.push_back(Json{{"type","apiKeySource"}});return input.value("apiKeySource",std::string("local"));};
  p.apiProvider=[&]{trace.push_back(Json{{"type","apiProvider"}});return input.value("apiProvider",std::string("firstParty"));};
  p.modelStrings=[&]{trace.push_back(Json{{"type","modelStrings"}});
    return input.value("modelStrings",Json{{"opus41","opus-4-1"},{"sonnet45","sonnet-4-5"},{"sonnet40","sonnet-4"}});};
  p.isDefaultPremiumModel=[&](const std::string&){trace.push_back(Json{{"type","isDefaultPremiumModel"}});
    return input.value("defaultPremiumModel",false);};
  p.rateLimitErrorMessage=[&](const Json& limits,const std::string& model) -> std::optional<std::string>{
    trace.push_back(Json{{"type","rateLimitErrorMessage"},{"limits",limits},{"model",model}});
    if(input.contains("rateLimitMessage")&&!input.at("rateLimitMessage").is_null())return input.at("rateLimitMessage").get<std::string>();
    return std::nullopt;
  };
  p.env=input.value("env",Json::object());
  const Json wire=input.at("wire");
  p.wire={wire.at("representativeClaim"),wire.at("overageStatus"),wire.at("reset"),wire.at("overageReset"),
    wire.at("overageDisabledReason"),wire.at("betaHeaderName")};
  p.policyURL=input.value("policyURL",std::string());
  p.preferredModel=input.value("preferredModel",std::string());
  p.statusURL=input.value("statusURL",std::string());
  p.usageURL=input.value("usageURL",std::string());
  p.betaHeaderValue=input.value("betaHeaderValue",std::string());
  const std::string op=input.at("op").get<std::string>();Json result;
  if(op=="refusal"){
    std::optional<std::string> reason;if(input.contains("stopReason")&&!input.at("stopReason").is_null())reason=input.at("stopReason").get<std::string>();
    auto r=getErrorMessageIfRefusal(reason,input.at("model").get<std::string>(),p);result=r?*r:Json(nullptr);
  }else if(op=="system"||op=="systemCause"){
    result=createSystemAPIErrorMessage(batbox::retry::errorFromJson(input.at("error")),
      input.at("retryInMs").get<double>(),input.at("retryAttempt").get<double>(),input.at("maxRetries").get<double>(),p);
    if(op=="systemCause")result=Json{{"hasCause",result.contains("cause")},{"type",result.at("type")},
      {"subtype",result.at("subtype")},{"level",result.at("level")},{"retryInMs",result.at("retryInMs")},
      {"retryAttempt",result.at("retryAttempt")},{"maxRetries",result.at("maxRetries")},
      {"timestamp",result.at("timestamp")},{"uuid",result.at("uuid")}};
  }
  else if(op=="render"){
    Failure f;f.error=batbox::retry::errorFromJson(input.at("error"));
    f.isErrorObject=input.value("isErrorObject",true);f.isConnectionTimeout=input.value("isConnectionTimeout",false);
    f.isImageSize=input.value("isImageSize",false);f.isImageResize=input.value("isImageResize",false);
    result=getAssistantMessageFromError(f,input.at("model").get<std::string>(),p);
  }else if(op=="categorize")result=categorizeRetryableAPIError(batbox::retry::errorFromJson(input.at("error")));
  else if(op=="format")result=formatAPIError(batbox::retry::errorFromJson(input.at("error")));
  else if(op=="valid")result=isValidAPIMessage(input.at("value"));
  else if(op=="unknownFormat"){
    auto x=extractUnknownErrorFormat(input.at("value"));result=x?Json(*x):Json(nullptr);
  }else if(op=="promptCounts")result=parsePoetryTooLongTokenCounts(input.at("text").get<std::string>());
  else if(op=="promptMessage")result=isPoetryTooLongMessage(input.at("message"));
  else if(op=="promptGap"){
    auto x=getPoetryTooLongTokenGap(input.at("message"));result=x?Json(*x):Json(nullptr);
  }else if(op=="media")result=isMediaSizeError(input.at("text").get<std::string>());
  else if(op=="mediaMessage")result=isMediaSizeErrorMessage(input.at("message"));
  else if(op=="prefix")result=startsWithApiErrorPrefix(input.at("text").get<std::string>());
  else throw std::runtime_error("unknown op");
  return Json{{"result",result},{"trace",trace}};
}
}
int main(){std::string line;while(std::getline(std::cin,line))try{std::cout<<evaluate(Json::parse(line)).dump()<<'\n';}
 catch(const std::exception& e){std::cout<<Json{{"fatal",e.what()}}.dump()<<'\n';}}
