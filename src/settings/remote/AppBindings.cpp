#include "settings/remote/AppBindings.hpp"
#include "cleanup/Signals.hpp"
#include "settings/remote/IndigenousFile.hpp"
#include <utility>

namespace batbox::settings::remote {
FetchOneServices bindIndigenousFetchOne(event_loop::Loop& loop,
    AuthServices auth,
    std::function<event_loop::Deferred<event_loop::Unit>()> refreshOAuth,
    std::function<std::string()> baseApiUrl,
    std::function<std::string()> userBat,
    const schema::MainConfig& schemaConfig,
    const PermissionRuleServices& permissionRules,
    std::function<void(const std::string&)> debug,
    std::string settingsPath){
  FetchOneServices services;
  services.refreshOAuth=std::move(refreshOAuth);
  services.auth=std::move(auth);
  services.baseApiUrl=std::move(baseApiUrl);
  services.userBat=std::move(userBat);
  services.settingsPath=std::move(settingsPath);
  services.httpGet=[&loop](const std::string& url,const Json& headers,
      double timeout){return indigenousHttpGet(loop,url,headers,timeout);};
  services.validateResponse=parseRemoteManagedSettingsResponse;
  services.validateSettings=schema::bindIndigenousSettingsSchema(schemaConfig,
    permissionRules);
  services.settingsErrorMessage=[](const SchemaResult& result){
    return value::stringify(result.issues,2);
  };
  services.debug=std::move(debug);
  return services;
}
RetryServices bindIndigenousRetry(event_loop::Loop& loop,
    FetchOneServices fetchOne,
    std::function<double(int)> getRetryDelay,
    std::function<void(const std::string&)> debug){
  RetryServices services;
  services.fetchOne=[&loop,fetchOne=std::move(fetchOne)](
      const std::optional<std::string>& checksum){
    return fetchRemoteManagedSettings(loop,checksum,fetchOne);
  };
  services.getRetryDelay=std::move(getRetryDelay);
  services.sleep=[&loop](double delay){
    event_loop::Deferred<event_loop::Unit> output(loop);
    loop.setTimeout(delay,[output](){output.resolve({});});
    return output;
  };
  services.debug=std::move(debug);
  return services;
}
FetchAndLoadServices bindIndigenousFetchAndLoad(event_loop::Loop& loop,
    SyncStash& stash,Eligibility& eligibility,RetryServices retry,
    SecurityServices security,
    std::function<void(const std::string&)> debug){
  FetchAndLoadServices services;
  services.stash=&stash;
  services.isEligible=[&eligibility](){
    return eligibility.isRemoteManagedSettingsEligible();};
  services.fetchWithRetry=[&loop,retry=std::move(retry)](
      const std::optional<std::string>& checksum){
    return fetchWithRetry(loop,checksum,retry);
  };
  services.checkSecurity=[&loop,security](const std::optional<Json>& stashed,
      const Json& next){
    return checkManagedSettingsSecurity(loop,stashed,next,security);
  };
  services.handleSecurity=[security](SecurityDecision decision){
    return handleSecurityCheckResult(decision,security);
  };
  services.saveSettings=[&loop,&stash,debug](const Json& next){
    return saveSettings(loop,next,[&stash](){return stash.getSettingsPath();},
      debug);
  };
  services.unlinkSettings=[&loop](const std::string& path){
    return unlinkSettings(loop,path);
  };
  services.debug=std::move(debug);
  return services;
}
LifecycleServices bindIndigenousLifecycle(event_loop::Loop& loop,
    SyncStash& stash,Eligibility& eligibility,
    FetchAndLoadServices fetchAndLoad,
    persistence::ChangeEvents& changes,cleanup::Roster& roster,
    std::function<void(const std::string&)> debug){
  LifecycleServices services;
  services.stash=&stash;services.eligibility=&eligibility;
  services.fetchAndLoad=[&loop,fetchAndLoad=std::move(fetchAndLoad)](){
    return fetchAndLoadRemoteManagedSettings(loop,fetchAndLoad);
  };
  services.unlinkSettings=[&loop](const std::string& path){
    return unlinkSettings(loop,path);
  };
  services.debug=std::move(debug);
  services.notifyPolicySettings=[&changes](){
    changes.notifyChange(Source::Policy);
  };
  services.setTimeout=[&loop](double delay,std::function<void()> callback){
    (void)loop.setTimeout(delay,std::move(callback));
  };
  services.setInterval=[&loop](double delay,std::function<void()> callback,
      bool unref){
    auto interval=cleanup::indigenousUnrefInterval(loop,delay,
      std::move(callback));
    if(unref)interval.unref();
    return interval.cancel;
  };
  services.registerCleanup=[&roster](auto callback){
    (void)roster.registerCleanup(std::move(callback));
  };
  return services;
}
void bindRemoteToLoader(SyncStash& stash,LoadServices& loader){
  loader.remoteManagedSettings=[&stash](){
    return stash.getRemoteManagedSettingsSyncFromStash();
  };
}
}
