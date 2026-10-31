#include "application_model/Internal.hpp"
#include "runtime/Platform.hpp"

namespace batbox::application_model {
retry::Dependencies LocalServiceBuilder::makeRetry(){
  retry::Dependencies deps;
  const auto& config=runtime.config;
  deps.env=config.environment;
  deps.wire=config.retryWire;
  deps.unattendedRetryFeature=config.features.contains("unattendedRetry");
  deps.bashClassifierFeature=config.features.contains("bashClassifier");
  deps.disableKeepAliveOnStaleConnection=true;
  deps.sleep=retry::sleepAbortResponsive;
  deps.random=platform::randomUnitInterval;
  deps.nowMs=[]{return static_cast<double>(platform::nowMillis());};
  deps.createSystemAPIErrorMessage=[errors=errors,messages=messages](
      const auto& error,
      double delay,double attempt,double max)mutable{
    errors.messageServices=&messages;
    return api_errors::createSystemAPIErrorMessage(error,delay,attempt,max,
      errors);
  };
  deps.isFastModeEnabled=[&host=host,enabled=config.fastMode]{
    std::lock_guard lock(host.mutex_);
    return enabled&&!host.state_.fastModeDisabled;
  };
  deps.isFastModeCooldown=[&host=host]{
    std::lock_guard lock(host.mutex_);
    return static_cast<double>(platform::nowMillis())<
      host.state_.fastModeCooldownUntilMs;
  };
  deps.handleFastModeOverageRejection=[&host=host](const auto& reason){
    if(reason!="org_level_disabled_until"&&reason!="out_of_credits"){
      host.imports_.disableFastModeInSettings();
      std::lock_guard lock(host.mutex_);host.state_.fastModeDisabled=true;
    }
    host.imports_.fastModeOverageRejection(reason);
    host.imports_.diagnostic("fast_mode_overage",reason);
  };
  deps.handleFastModeRejectedByAPI=[&host=host]{
    host.imports_.disableFastModeInSettings();
    {std::lock_guard lock(host.mutex_);host.state_.fastModeDisabled=true;}
    host.imports_.diagnostic("fast_mode_rejected","api");
  };
  deps.triggerFastModeCooldown=[&host=host](double until,const auto& reason){
    {std::lock_guard lock(host.mutex_);
      host.state_.fastModeCooldownUntilMs=until;}
    host.imports_.diagnostic("fast_mode_cooldown",
      reason+":"+value::numberToString(until));
  };
  deps.getOAuthAccessToken=[]() -> std::optional<std::string>{return {};};
  deps.clearApiKeyHelperStash=host.imports_.invalidateAuthToken;
  deps.isSubscriber=[enabled=config.subscriber]{return enabled;};
  deps.isEnterpriseSubscriber=[enabled=config.enterpriseSubscriber]{
    return enabled;
  };
  deps.isDefaultPremiumModel=[main=config.mainModel](const auto& name){
    return name==main;
  };
  deps.disableKeepAlive=[&host=host]{
    std::lock_guard lock(host.mutex_);host.state_.keepAliveDisabled=true;
  };
  deps.logForDebugging=[&host=host](const auto& value){
    host.imports_.debug("debug",value);
  };
  deps.logError=[&host=host](const auto& value){
    host.imports_.debug("error",value);
  };
  return deps;
}
} // namespace batbox::application_model
