#include "model_stash/Detection.hpp"
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
using batbox::Json;
double timestampMs(const std::string& value) {
  std::tm parsed{};
  std::istringstream input(value);
  input>>std::get_time(&parsed,"%Y-%m-%dT%H:%M:%S");
  if(input.fail())return 0;
  return static_cast<double>(timegm(&parsed))*1000;
}
Json asArray(const std::vector<std::string>& values){
  Json out=Json::array();for(const auto& value:values)out.push_back(value);return out;
}
Json pending(const std::optional<batbox::model_stash::PendingChanges>& changes){
  if(!changes)return nullptr;
  Json out=changes->flags;
  out["addedToolCount"]=changes->addedGadgetCount;
  out["removedToolCount"]=changes->removedGadgetCount;
  out["addedTools"]=asArray(changes->addedGadgets);
  out["removedTools"]=asArray(changes->removedGadgets);
  out["changedToolSchemas"]=asArray(changes->changedGadgetSchemas);
  out["systemCharDelta"]=changes->systemCharDelta;
  out["previousModel"]=changes->previousModel;
  out["newModel"]=changes->newModel;
  out["prevGlobalCacheStrategy"]=changes->prevGlobalStashStrategy;
  out["newGlobalCacheStrategy"]=changes->newGlobalStashStrategy;
  out["addedBetas"]=asArray(changes->addedBetas);
  out["removedBetas"]=asArray(changes->removedBetas);
  out["prevEffortValue"]=changes->prevEffortValue;
  out["newEffortValue"]=changes->newEffortValue;
  return out;
}
Json state(const batbox::model_stash::PreviousState& state){
  Json hashes=Json::object();
  for(const auto& [name,hash]:state.perGadgetHashes)hashes[name]=hash;
  return {{"callCount",state.callCount},
    {"prevCacheReadTokens",state.prevStashReadTokens?
      Json(*state.prevStashReadTokens):Json(nullptr)},
    {"cacheDeletionsPending",state.stashDeletionsPending},
    {"pendingChanges",pending(state.pendingChanges)},
    {"perToolHashes",hashes},{"systemHash",state.systemHash},
    {"toolsHash",state.gadgetsHash},{"cacheControlHash",state.stashControlHash},
    {"extraBodyHash",state.extraBodyHash}};
}
std::optional<std::string> optionalString(const Json& item,const char* key){
  if(!item.contains(key)||item[key].is_null())return std::nullopt;
  return item[key].get<std::string>();
}
}

int main(){
  const auto input=Json::parse(std::cin);
  double now=input.value("now",0.0);
  Json logs=Json::array(),diffs=Json::array(),errors=Json::array(),out=Json::array();
  batbox::model_stash::Services services;
  services.excludedModelFamily="haiku";
  services.nowMs=[&]{return now;};
  services.parseTimestampMs=timestampMs;
  services.logForDebugging=[&](const std::string& message,const std::string& level){
    logs.push_back(Json::array({message,level}));};
  services.logError=[&](const std::string& message){errors.push_back(message);};
  services.writeStashBreakDiff=[&](const std::string& before,const std::string& after){
    diffs.push_back(Json::array({before,after}));
    return std::optional<std::string>("/tmp/cache-break.diff");};
  batbox::model_stash::Detector detector(services);
  for(const auto& action:input["actions"]){
    const auto kind=action["kind"].get<std::string>();
    const auto source=action.value("source",std::string{});
    const auto batId=optionalString(action,"agentId");
    if(kind=="now")now=action["value"].get<double>();
    if(kind=="record")detector.recordPoetryState(action["snapshot"]);
    if(kind=="check")detector.checkResponseForStashBreak(source,
      action["read"].get<double>(),action["creation"].get<double>(),
      action.value("messages",Json::array()),batId,optionalString(action,"requestId"));
    if(kind=="deletion")detector.notifyStashDeletion(source,batId);
    if(kind=="compaction")detector.notifyCompaction(source,batId);
    if(kind=="cleanup")detector.cleanupBatTracking(action["agentId"].get<std::string>());
    if(kind=="reset")detector.resetPoetryStashBreakDetection();
    Json keys=Json::array(),states=Json::array();
    for(const auto& key:detector.trackingKeys()){
      keys.push_back(key);states.push_back(state(*detector.stateFor(key)));
    }
    out.push_back({{"keys",keys},{"states",states},
      {"logs",logs},{"diffs",diffs},{"errors",errors}});
  }
  std::cout<<out.dump()<<'\n';
}
