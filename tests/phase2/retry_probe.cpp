#include "retry/Retry.hpp"
#include <cmath>
#include <iostream>
#include <optional>
#include <stop_token>
#include <string>
#include <vector>
using batbox::Json;
using namespace batbox::retry;
namespace {
Json number(double n) { return std::isfinite(n) ? Json(n) : Json(nullptr); }
Json evaluate(const Json& input) {
  std::vector<Json> trace;
  std::stop_source stop;
  Dependencies d;
  d.env=input.value("env",Json::object());
  const auto wire=input.at("wire");
  d.wire={wire.at("retryAfter").get<std::string>(),wire.at("shouldRetry").get<std::string>(),
    wire.at("overageDisabledReason").get<std::string>(),wire.at("rateLimitReset").get<std::string>()};
  d.unattendedRetryFeature=input.value("unattendedFeature",false);
  d.bashClassifierFeature=input.value("bashClassifierFeature",false);
  d.disableKeepAliveOnStaleConnection=input.value("disableKeepAliveOnStaleConnection",false);
  int clientCount=0, operationCount=0, sleepCount=0, yieldCount=0;
  bool fastEnabled=input.value("fastEnabled",false);
  bool cooldown=input.value("fastCooldown",false);
  d.getClient=[&] { trace.push_back(Json{{"type","getClient"}}); return Json{{"client",++clientCount}}; };
  d.operation=[&](const Json& client,double attempt,const RetryContext& context) -> Json {
    trace.push_back(Json{{"type","operation"},{"client",client},{"attempt",number(attempt)},{"context",contextToJson(context)}});
    const Json responses=input.value("responses",Json::array());
    const Json response=operationCount<static_cast<int>(responses.size()) ? responses[operationCount++] : Json{{"result",Json{{"ok",true}}}};
    if (response.contains("error")) throw RetryFailure(errorFromJson(response.at("error")));
    return response.value("result",Json());
  };
  d.yield=[&](const Json& message){trace.push_back(Json{{"type","yield"},{"message",message}});
    if (input.contains("abortAtYield") && ++yieldCount==input.at("abortAtYield").get<int>()) stop.request_stop();
  };
  d.createSystemAPIErrorMessage=[](const ErrorRecord& error,double remaining,double attempt,double maxRetries) {
    Json m={{"status",error.status ? Json(*error.status) : Json(nullptr)},
      {"message",error.message},{"remaining",number(remaining)},
      {"attempt",number(attempt)},{"maxRetries",number(maxRetries)}};
    return m;
  };
  d.sleep=[&](double ms,std::stop_token) { trace.push_back(Json{{"type","sleep"},{"ms",number(ms)}});
    if (input.contains("abortAtSleep") && ++sleepCount==input.at("abortAtSleep").get<int>()) stop.request_stop();
  };
  d.random=[&] { trace.push_back(Json{{"type","random"}}); return input.value("random",0.25); };
  d.nowMs=[&] { trace.push_back(Json{{"type","now"}}); return input.value("nowMs",1000000.0); };
  d.isFastModeEnabled=[&] { return fastEnabled; };
  d.isFastModeCooldown=[&] { return cooldown; };
  d.handleFastModeOverageRejection=[&](const std::string& reason) {
    trace.push_back(Json{{"type","overage"},{"reason",reason}}); fastEnabled=false;
  };
  d.handleFastModeRejectedByAPI=[&] { trace.push_back(Json{{"type","fastRejected"}}); fastEnabled=false; };
  d.triggerFastModeCooldown=[&](double until,const std::string& reason) {
    trace.push_back(Json{{"type","cooldown"},{"until",number(until)},{"reason",reason}}); cooldown=true;
  };
  d.getOAuthAccessToken=[&]() -> std::optional<std::string> {
    if (input.contains("accessToken")) return input.at("accessToken").get<std::string>();
    return std::nullopt;
  };
  d.handleOAuth401Error=[&](const std::string& token) { trace.push_back(Json{{"type","oauthRefresh"},{"token",token}}); };
  d.clearApiKeyHelperStash=[&] { trace.push_back(Json{{"type","clearApiKey"}}); };
  d.clearAwsCredentialsStash=[&] { trace.push_back(Json{{"type","clearAws"}}); };
  d.clearGcpCredentialsStash=[&] { trace.push_back(Json{{"type","clearGcp"}}); };
  d.isSubscriber=[&] { return input.value("subscriber",false); };
  d.isEnterpriseSubscriber=[&] { return input.value("enterprise",false); };
  d.isDefaultPremiumModel=[&](const std::string&) { return input.value("defaultPremiumModel",false); };
  d.disableKeepAlive=[&] { trace.push_back(Json{{"type","disableKeepAlive"}}); };
  d.logForDebugging=[&](const std::string& msg) { trace.push_back(Json{{"type","debug"},{"message",msg}}); };
  d.logError=[&](const std::string& msg) { trace.push_back(Json{{"type","logError"},{"message",msg}}); };
  const std::string op=input.at("op").get<std::string>();
  try {
    Json result;
    if (op=="withRetry") {
      RetryOptions o;
      const Json options=input.at("options");
      o.model=options.at("model").get<std::string>();
      o.thinkingConfig=options.value("thinkingConfig",Json{{"type","disabled"}});
      if (options.contains("maxRetries")) o.maxRetries=options.at("maxRetries").get<double>();
      if (options.contains("fallbackModel")) o.fallbackModel=options.at("fallbackModel").get<std::string>();
      if (options.contains("fastMode")) o.fastMode=options.at("fastMode").get<bool>();
      if (options.contains("querySource")) o.querySource=options.at("querySource").get<std::string>();
      o.initialConsecutive529Errors=options.value("initialConsecutive529Errors",0);
      o.signal=stop.get_token();
      if (input.value("initialAbort",false)) stop.request_stop();
      result=withRetry(o,d);
    } else if (op=="retryDelay") {
      std::optional<std::string> h;
      if (input.contains("retryAfter") && !input.at("retryAfter").is_null()) h=input.at("retryAfter").get<std::string>();
      result=number(getRetryDelay(input.at("attempt").get<double>(),h,input.value("maxDelayMs",32000.0),d));
    } else if (op=="shouldRetry") result=shouldRetry(errorFromJson(input.at("error")),d);
    else if (op=="is529") result=is529Error(errorFromJson(input.at("error")));
    else if (op=="parseOverflow") {
      auto x=parseMaxTokensContextOverflowError(errorFromJson(input.at("error")),d);
      result=x ? Json{{"inputTokens",number(x->inputTokens)},{"maxTokens",number(x->maxTokens)},{"contextLimit",number(x->contextLimit)}} : Json(nullptr);
    } else if (op=="shouldRetry529") {
      std::optional<std::string> q;
      if (input.contains("querySource")) q=input.at("querySource").get<std::string>();
      result=shouldRetry529(q,d.bashClassifierFeature);
    } else if (op=="getDefaultMaxRetries") result=number(getDefaultMaxRetries(d));
    else if (op=="extractConnectionDetails") {
      auto x=extractConnectionErrorDetails(errorFromJson(input.at("error")));
      result=x ? Json{{"code",x->code},{"message",x->message},{"isSSLError",x->isSSLError}} : Json(nullptr);
    } else throw std::runtime_error("unknown op");
    return Json{{"result",result},{"trace",trace}};
  } catch (const CannotRetryError& e) {
    Json out={{"errorClass","CannotRetryError"},{"name","RetryError"},{"message",e.what()},
      {"context",contextToJson(e.retryContext)},{"trace",trace}};
    if (e.originalError) out["original"]={{"kind",errorToJson(*e.originalError).at("kind")},
      {"status",e.originalError->status ? Json(*e.originalError->status) : Json(nullptr)},
      {"message",e.originalError->message}};
    return out;
  } catch (const FallbackTriggeredError& e) {
    return Json{{"errorClass","FallbackTriggeredError"},{"name","FallbackTriggeredError"},
      {"message",e.what()},{"originalModel",e.originalModel},{"fallbackModel",e.fallbackModel},{"trace",trace}};
  } catch (const RetryFailure& e) {
    return Json{{"errorClass",e.error.name},{"name",e.error.name},{"message",e.error.message},
      {"kind",errorToJson(e.error).at("kind")},{"trace",trace}};
  }
}
}
int main(){std::string line;while(std::getline(std::cin,line))try{std::cout<<evaluate(Json::parse(line)).dump()<<'\n';}catch(const std::exception& e){std::cout<<Json{{"fatal",e.what()}}.dump()<<'\n';}}
