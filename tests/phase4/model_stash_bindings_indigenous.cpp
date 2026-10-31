#include "model_stash/ModelBindings.hpp"
#include "model_stash/Date.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>

int main(){
  using batbox::Json;
  batbox::event_loop::Loop loop;
  int stashWarnings=0,quotaChanges=0;
  batbox::model_stash::Services stashServices;
  stashServices.excludedModelFamily="fast";
  stashServices.nowMs=[](){return 100000.0;};
  stashServices.parseTimestampMs=batbox::model_stash::indigenousMessageTimestampMs;
  stashServices.logForDebugging=[&](const std::string&,const std::string& level){
    if(level=="warn")++stashWarnings;};
  stashServices.logError=[](const std::string&){assert(false);};
  auto stash=std::make_shared<batbox::model_stash::Detector>(stashServices);
  Json config=Json::object();
  batbox::quota::Services quotaServices;
  quotaServices.isSubscriber=[](){return true;};
  quotaServices.shouldProcessRateLimits=[](bool){return true;};
  quotaServices.processRateLimitHeaders=[](const Json& headers){return headers;};
  quotaServices.getGlobalConfig=[&](){return config;};
  quotaServices.saveGlobalConfig=[&](const Json& next){config=next;};
  quotaServices.nowSeconds=[](){return 100000.0;};
  auto quota=std::make_shared<batbox::quota::QuotaState>(
    batbox::quota::Wire{"limit-"},quotaServices);
  quota->addStatusListener([&](const Json&){++quotaChanges;});
  const auto temp=std::filesystem::temp_directory_path()/"batbox-cache-binding-native";
  batbox::model::QueryServices query;
  batbox::model_stash::bindQueryDiagnostics(query,loop,stash,quota,
    {[temp](){return temp.string();},[](){return 0.0;}});
  auto snapshot=[](const std::string& text){return Json{{"system",Json::array({
      Json{{"text",text}}})},{"toolSchemas",Json::array()},
      {"querySource","sdk"},{"model","standard"}};};
  query.preflight.recordPoetryState(snapshot("old"));
  query.postStream.checkResponseForStashBreak("sdk",10000,0,Json::array(),
    nullptr,std::nullopt);
  query.preflight.recordPoetryState(snapshot("new"));
  query.postStream.checkResponseForStashBreak("sdk",1000,9000,Json::array(),
    nullptr,std::nullopt);
  query.postStream.extractQuotaStatusFromHeaders(Json{
    {"limit-status","allowed_warning"},
    {"limit-5h-surpassed-threshold","0.9"}});
  assert(quota->currentLimits()["status"]=="allowed_warning");
  batbox::retry::ErrorRecord error;
  error.kind=batbox::retry::ErrorRecord::Kind::Api;
  error.status=429;error.headers=nullptr;
  query.extractQuotaStatusFromError(error);
  assert(quota->currentLimits()["status"]=="rejected");
  loop.run();
  assert(stashWarnings==1&&quotaChanges==2);
  std::filesystem::remove_all(temp);
  std::cout<<"query cache/quota callbacks bind to shared state\n";
}
