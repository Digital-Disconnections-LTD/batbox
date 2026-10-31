#pragma once

#include "retry/Retry.hpp"
#include <functional>
#include <string>
#include <vector>

namespace batbox::quota {

// Wire spellings are supplied by the backend adapter. Claim names are
// converted to Batbox's fast/standard/premium vocabulary at this boundary.
struct Wire {
  std::string prefix;
  std::string fiveHourClaim = "5h";
  std::string sevenDayClaim = "7d";
  std::string overageClaim = "overage";
  std::string premiumClaim;
  std::string standardClaim;
};

struct Services {
  std::function<bool()> isSubscriber;
  std::function<bool(bool)> shouldProcessRateLimits;
  std::function<Json(const Json&)> processRateLimitHeaders;
  std::function<Json()> getGlobalConfig;
  std::function<void(const Json&)> saveGlobalConfig;
  std::function<double()> nowSeconds;
  std::function<void(const std::string&)> logError;
};

class QuotaState {
public:
  using Listener = std::function<void(const Json&)>;
  QuotaState(Wire wire, Services services);
  void extractQuotaStatusFromHeaders(const Json& headers);
  void extractQuotaStatusFromError(const retry::ErrorRecord& error);
  const Json& currentLimits() const { return limits_; }
  const Json& rawUtilization() const { return rawUtilization_; }
  void addStatusListener(Listener listener);

private:
  Wire wire_;
  Services services_;
  Json limits_;
  Json rawUtilization_ = Json::object();
  std::vector<Listener> listeners_;
  void emitStatusChange(const Json& limits);
  Json extractRawUtilization(const Json& headers) const;
  Json computeNewLimitsFromHeaders(const Json& headers) const;
  Json getEarlyWarningFromHeaders(const Json& headers,bool fallback) const;
  Json getHeaderBasedEarlyWarning(const Json& headers,bool fallback) const;
  Json getTimeRelativeEarlyWarning(const Json& headers,
      const std::string& claim,const std::string& type,double windowSeconds,
      const std::vector<std::pair<double,double>>& thresholds,bool fallback) const;
  void stashExtraUsageDisabledReason(const Json& headers);
  std::string header(const std::string& suffix) const;
};

} // namespace batbox::quota
