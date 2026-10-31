#include "quota/Quota.hpp"
#include <iostream>

int main(){
  using batbox::Json;
  auto input=Json::parse(std::cin);
  bool enabled=true;
  double now=input.value("now",0.0);
  Json config=Json::object(),events=Json::array(),snapshots=Json::array();
  batbox::quota::Wire wire;
  wire.prefix="anthropic-ratelimit-unified-";
  batbox::quota::Services services;
  services.isSubscriber=[](){return true;};
  services.shouldProcessRateLimits=[&](bool){return enabled;};
  services.processRateLimitHeaders=[](const Json& headers){return headers;};
  services.getGlobalConfig=[&](){return config;};
  services.saveGlobalConfig=[&](const Json& next){config=next;};
  services.nowSeconds=[&](){return now;};
  batbox::quota::QuotaState state(wire,services);
  state.addStatusListener([&](const Json& limits){events.push_back(limits);});
  for(const auto& action:input["actions"]){
    auto kind=action["kind"].get<std::string>();
    if(kind=="enabled")enabled=action["value"].get<bool>();
    if(kind=="now")now=action["value"].get<double>();
    if(kind=="headers")state.extractQuotaStatusFromHeaders(action["headers"]);
    if(kind=="error"){
      batbox::retry::ErrorRecord error;
      error.kind=batbox::retry::ErrorRecord::Kind::Api;
      error.status=action["status"].get<long>();
      if(action.contains("headers"))error.headers=action["headers"];
      else error.headers=nullptr;
      state.extractQuotaStatusFromError(error);
    }
    snapshots.push_back({{"limits",state.currentLimits()},
      {"raw",state.rawUtilization()},{"config",config},{"events",events}});
  }
  std::cout<<snapshots.dump()<<'\n';
}
