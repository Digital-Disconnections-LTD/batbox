#include "quota/Quota.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace batbox::quota {
namespace {

const Json& field(const Json& object,const std::string& key) {
  static const Json missing = nullptr;
  if(!object.is_object())return missing;
  auto found=object.find(key);
  return found==object.end()?missing:*found;
}
std::string value(const Json& object,const std::string& key) {
  const auto& item=field(object,key);
  return item.is_string()?item.get<std::string>():std::string{};
}
bool present(const Json& object,const std::string& key) {
  return object.is_object() && object.contains(key) && !object[key].is_null();
}
bool truthyString(const std::string& s) { return !s.empty(); }
bool truthyValue(const Json& value) {
  if(value.is_null()||value.is_boolean()&& !value.get<bool>())return false;
  if(value.is_number())return value.get<double>()!=0;
  if(value.is_string())return !value.get<std::string>().empty();
  return true;
}
double number(const std::string& s) {
  if(s.empty())return 0;
  char* end=nullptr;
  auto converted=std::strtod(s.c_str(),&end);
  return end==s.c_str() || *end!='\0' ? std::numeric_limits<double>::quiet_NaN() : converted;
}
void setNumber(Json& object,const std::string& key,double n) {
  // JSON is the indigenous adapter boundary; JavaScript's NaN becomes null when
  // serialized, as it does in the persisted statusline state.
  object[key]=std::isfinite(n)?Json(n):Json(nullptr);
}
bool equal(const Json& a,const Json& b) {
  if(a.is_object()&&b.is_object()) {
    if(a.size()!=b.size())return false;
    for(auto i=a.begin();i!=a.end();++i) {
      if(!b.contains(i.key())||!equal(i.value(),b[i.key()]))return false;
    }
    return true;
  }
  if(a.is_array()&&b.is_array()) {
    if(a.size()!=b.size())return false;
    for(std::size_t i=0;i<a.size();++i)if(!equal(a[i],b[i]))return false;
    return true;
  }
  return a==b;
}
void require(const Services& services) {
  if(!services.isSubscriber||!services.shouldProcessRateLimits||
      !services.processRateLimitHeaders||!services.getGlobalConfig||
      !services.saveGlobalConfig||!services.nowSeconds)
    throw std::invalid_argument("Quota service imports are required");
}
}

QuotaState::QuotaState(Wire wire,Services services)
    : wire_(std::move(wire)),services_(std::move(services)),
      limits_({{"status","allowed"},{"unifiedRateLimitFallbackAvailable",false},
        {"isUsingOverage",false}}) {
  require(services_);
  if(wire_.prefix.empty())throw std::invalid_argument("Quota header prefix is required");
}

std::string QuotaState::header(const std::string& suffix) const {
  return wire_.prefix+suffix;
}

void QuotaState::addStatusListener(Listener listener) {
  listeners_.push_back(std::move(listener));
}
void QuotaState::emitStatusChange(const Json& limits) {
  limits_=limits;
  for(auto& listener:listeners_)listener(limits);
  // Status changes do not upload analytics.
}

Json QuotaState::extractRawUtilization(const Json& headers) const {
  Json result=Json::object();
  for(const auto& [key,claim]:std::vector<std::pair<std::string,std::string>>{
      {"five_hour",wire_.fiveHourClaim},{"seven_day",wire_.sevenDayClaim}}) {
    auto util=field(headers,header(claim+"-utilization"));
    auto reset=field(headers,header(claim+"-reset"));
    if(!util.is_null()&&!reset.is_null()) {
      Json window=Json::object();
      setNumber(window,"utilization",number(util.get<std::string>()));
      setNumber(window,"resets_at",number(reset.get<std::string>()));
      result[key]=std::move(window);
    }
  }
  return result;
}

Json QuotaState::getHeaderBasedEarlyWarning(const Json& headers,bool fallback) const {
  for(const auto& [claim,type]:std::vector<std::pair<std::string,std::string>>{
      {wire_.fiveHourClaim,"five_hour"},{wire_.sevenDayClaim,"seven_day"},
      {wire_.overageClaim,"overage"}}) {
    auto threshold=field(headers,header(claim+"-surpassed-threshold"));
    if(threshold.is_null())continue;
    const auto util=value(headers,header(claim+"-utilization"));
    const auto reset=value(headers,header(claim+"-reset"));
    Json result={{"status","allowed_warning"},
      {"rateLimitType",type},
      {"unifiedRateLimitFallbackAvailable",fallback},
      {"isUsingOverage",false}};
    if(truthyString(util))setNumber(result,"utilization",number(util));
    if(truthyString(reset))setNumber(result,"resetsAt",number(reset));
    setNumber(result,"surpassedThreshold",number(threshold.get<std::string>()));
    return result;
  }
  return nullptr;
}

Json QuotaState::getTimeRelativeEarlyWarning(const Json& headers,
    const std::string& claim,const std::string& type,double windowSeconds,
    const std::vector<std::pair<double,double>>& thresholds,bool fallback) const {
  const auto util=field(headers,header(claim+"-utilization"));
  const auto reset=field(headers,header(claim+"-reset"));
  if(util.is_null()||reset.is_null())return nullptr;
  const double utilization=number(util.get<std::string>());
  const double resetsAt=number(reset.get<std::string>());
  const double elapsed=services_.nowSeconds()-(resetsAt-windowSeconds);
  const double progress=std::max(0.0,std::min(1.0,elapsed/windowSeconds));
  bool warn=false;
  for(auto [u,p]:thresholds)if(utilization>=u&&progress<=p){warn=true;break;}
  if(!warn)return nullptr;
  Json result={{"status","allowed_warning"},{"rateLimitType",type},
    {"unifiedRateLimitFallbackAvailable",fallback},{"isUsingOverage",false}};
  setNumber(result,"resetsAt",resetsAt);
  setNumber(result,"utilization",utilization);
  return result;
}

Json QuotaState::getEarlyWarningFromHeaders(const Json& headers,bool fallback) const {
  auto warning=getHeaderBasedEarlyWarning(headers,fallback);
  if(!warning.is_null())return warning;
  warning=getTimeRelativeEarlyWarning(headers,wire_.fiveHourClaim,"five_hour",
      5*60*60,{{0.9,0.72}},fallback);
  if(!warning.is_null())return warning;
  return getTimeRelativeEarlyWarning(headers,wire_.sevenDayClaim,"seven_day",
      7*24*60*60,{{0.75,0.6},{0.5,0.35},{0.25,0.15}},fallback);
}

Json QuotaState::computeNewLimitsFromHeaders(const Json& headers) const {
  auto status=value(headers,header("status"));
  if(status.empty())status="allowed";
  auto reset=value(headers,header("reset"));
  const bool fallback=value(headers,header("fallback"))=="available";
  auto claim=value(headers,header("representative-claim"));
  if(claim==wire_.premiumClaim&&!wire_.premiumClaim.empty())claim="seven_day_premium";
  else if(claim==wire_.standardClaim&&!wire_.standardClaim.empty())claim="seven_day_standard";
  auto overage=value(headers,header("overage-status"));
  auto overageReset=value(headers,header("overage-reset"));
  auto disabled=value(headers,header("overage-disabled-reason"));
  const bool usingOverage=status=="rejected" &&
      (overage=="allowed"||overage=="allowed_warning");
  if(status=="allowed"||status=="allowed_warning") {
    auto warning=getEarlyWarningFromHeaders(headers,fallback);
    if(!warning.is_null())return warning;
    status="allowed";
  }
  Json result={{"status",status},{"unifiedRateLimitFallbackAvailable",fallback}};
  if(!reset.empty())setNumber(result,"resetsAt",number(reset));
  if(!claim.empty())result["rateLimitType"]=claim;
  if(!overage.empty())result["overageStatus"]=overage;
  if(!overageReset.empty()&&number(overageReset)!=0)
    setNumber(result,"overageResetsAt",number(overageReset));
  if(!disabled.empty())result["overageDisabledReason"]=disabled;
  result["isUsingOverage"]=usingOverage;
  return result;
}

void QuotaState::stashExtraUsageDisabledReason(const Json& headers) {
  auto reason=field(headers,header("overage-disabled-reason"));
  auto config=services_.getGlobalConfig();
  const bool stashedPresent=config.is_object()&&config.contains("cachedExtraUsageDisabledReason");
  if(!stashedPresent||!equal(field(config,"cachedExtraUsageDisabledReason"),reason)) {
    if(!config.is_object())config=Json::object();
    config["cachedExtraUsageDisabledReason"]=reason;
    services_.saveGlobalConfig(config);
  }
}

void QuotaState::extractQuotaStatusFromHeaders(const Json& headers) {
  if(!services_.shouldProcessRateLimits(services_.isSubscriber())) {
    rawUtilization_=Json::object();
    if(value(limits_,"status")!="allowed"||truthyValue(field(limits_,"resetsAt")))
      emitStatusChange({{"status","allowed"},
        {"unifiedRateLimitFallbackAvailable",false},{"isUsingOverage",false}});
    return;
  }
  auto effective=services_.processRateLimitHeaders(headers);
  rawUtilization_=extractRawUtilization(effective);
  auto next=computeNewLimitsFromHeaders(effective);
  stashExtraUsageDisabledReason(effective);
  if(!equal(limits_,next))emitStatusChange(next);
}

void QuotaState::extractQuotaStatusFromError(const retry::ErrorRecord& error) {
  if(!services_.shouldProcessRateLimits(services_.isSubscriber())||
      !error.status||*error.status!=429)return;
  try {
    auto next=limits_;
    if(!error.headers.is_null()) {
      auto effective=services_.processRateLimitHeaders(error.headers);
      rawUtilization_=extractRawUtilization(effective);
      next=computeNewLimitsFromHeaders(effective);
      stashExtraUsageDisabledReason(effective);
    }
    next["status"]="rejected";
    if(!equal(limits_,next))emitStatusChange(next);
  } catch(const std::exception& e) {
    if(services_.logError)services_.logError(e.what());
  }
}

} // namespace batbox::quota
