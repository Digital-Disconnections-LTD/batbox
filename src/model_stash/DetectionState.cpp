#include "model_stash/Detection.hpp"
#include <algorithm>
#include <cstdint>
#include <set>
#include <stdexcept>

namespace batbox::model_stash {
namespace {

const Json& field(const Json& obj,const char* name) {
  return value::property(obj,name);
}
std::string string(const Json& value,const std::string& fallback="") {
  if(value.is_null())return fallback;
  if(value.is_string())return value.get<std::string>();
  if(value.is_boolean())return value.get<bool>()?"true":"false";
  if(value.is_number())return value::numberToString(value.get<double>());
  return value::stringify(value);
}
std::string value(const Json& obj,const char* name,const std::string& fallback="") {
  return string(field(obj,name),fallback);
}
Json stripStashControl(const Json& items) {
  Json result=Json::array();
  for(const auto& item:items) {
    if(!item.is_object()||!item.contains("cache_control")) {
      result.push_back(item);continue;
    }
    Json stripped=Json::object();
    for(auto it=item.begin();it!=item.end();++it)
      if(it.key()!="cache_control")stripped[it.key()]=it.value();
    result.push_back(std::move(stripped));
  }
  return result;
}
std::int32_t computeHash(const Json& data) {
  const auto serialized=value::stringify(data);
  std::uint32_t hash=0;
  for(const auto unit:value::utf16CodeUnits(serialized))
    hash=hash*31u+static_cast<std::uint32_t>(unit);
  return static_cast<std::int32_t>(hash);
}
std::vector<std::string> arrayStrings(const Json& items) {
  std::vector<std::string> result;
  if(!items.is_array())return result;
  for(const auto& item:items)result.push_back(string(item,"unknown"));
  return result;
}
std::map<std::string,std::int32_t> perGadgetHashes(const Json& stripped,
    const std::vector<std::string>& names) {
  std::map<std::string,std::int32_t> result;
  for(std::size_t i=0;i<stripped.size();++i)
    result[i<names.size()?names[i]:"__idx_"+std::to_string(i)]=computeHash(stripped[i]);
  return result;
}
int systemCharCount(const Json& system) {
  int total=0;
  for(const auto& block:system)
    total+=static_cast<int>(value::stringLength(value(block,"text")));
  return total;
}
}
std::string buildDiffableContent(const Json& system,const Json& gadgets,
    const std::string& model) {
  std::string systemText;
  for(std::size_t i=0;i<system.size();++i){
    if(i)systemText+="\n\n";
    systemText+=value(system[i],"text");
  }
  std::vector<std::string> details;
  for(const auto& gadget:gadgets) {
    if(!gadget.is_object()||!gadget.contains("name")){
      details.push_back("unknown");continue;
    }
    const auto description=gadget.contains("description")?
      string(gadget["description"],"undefined"):"";
    const auto schema=gadget.contains("input_schema")?
      value::stringify(gadget["input_schema"]):"";
    details.push_back(string(gadget["name"])+"\n  description: "+description+
      "\n  input_schema: "+schema);
  }
  std::sort(details.begin(),details.end());
  std::string gadgetDetails;
  for(std::size_t i=0;i<details.size();++i){
    if(i)gadgetDetails+="\n\n";
    gadgetDetails+=details[i];
  }
  return "Model: "+model+"\n\n=== System Prompt ===\n\n"+systemText+
    "\n\n=== Tools ("+std::to_string(gadgets.size())+") ===\n\n"+
    gadgetDetails+"\n";
}
namespace {
std::vector<std::string> difference(const std::vector<std::string>& a,
    const std::set<std::string>& b) {
  std::vector<std::string> out;
  for(const auto& item:a)if(!b.contains(item))out.push_back(item);
  return out;
}
}

bool PendingChanges::flag(const char* name) const {
  auto found=flags.find(name);
  return found!=flags.end()&&found->is_boolean()&&found->get<bool>();
}
Detector::Detector(Services services):services_(std::move(services)) {
  if(!services_.nowMs||!services_.parseTimestampMs||
      !services_.logForDebugging||!services_.logError||
      services_.excludedModelFamily.empty())
    throw std::invalid_argument("Cache detector imports are required");
}
std::optional<std::string> Detector::trackingKey(const std::string& querySource,
    const std::optional<std::string>& batId) const {
  if(querySource=="compact")return "repl_main_thread";
  for(const std::string prefix:{"repl_main_thread","sdk","agent:custom",
      "agent:default","agent:builtin"}) {
    if(querySource.starts_with(prefix))
      return batId&& !batId->empty()?*batId:querySource;
  }
  return std::nullopt;
}
void Detector::erase(const std::string& key) {
  states_.erase(key);
  insertionOrder_.remove(key);
}
std::optional<PreviousState> Detector::stateFor(const std::string& key) const {
  const auto found=states_.find(key);
  return found==states_.end()?std::nullopt:std::optional(*found->second);
}
std::vector<std::string> Detector::trackingKeys() const {
  return {insertionOrder_.begin(),insertionOrder_.end()};
}
void Detector::recordPoetryState(const Json& snapshot) {
  try {
    const auto& system=field(snapshot,"system");
    const auto& gadgetSchemas=field(snapshot,"toolSchemas");
    const auto querySource=value(snapshot,"querySource");
    const auto model=value(snapshot,"model");
    const auto batId=field(snapshot,"agentId").is_null()?std::nullopt:
      std::optional(value(snapshot,"agentId"));
    const auto key=trackingKey(querySource,batId);
    if(!key)return;

    const auto strippedSystem=stripStashControl(system);
    const auto strippedGadgets=stripStashControl(gadgetSchemas);
    const auto systemHash=computeHash(strippedSystem);
    const auto gadgetsHash=computeHash(strippedGadgets);
    Json controls=Json::array();
    for(const auto& block:system)
      controls.push_back(field(block,"cache_control"));
    const auto stashControlHash=computeHash(controls);
    std::vector<std::string> gadgetNames;
    for(const auto& gadget:gadgetSchemas)
      gadgetNames.push_back(gadget.is_object()&&gadget.contains("name")?
        string(gadget["name"]):"unknown");
    const auto count=systemCharCount(system);
    const bool fastMode=value::truthy(field(snapshot,"fastMode"));
    const bool autoMode=value::truthy(field(snapshot,"autoModeActive"));
    const bool overage=value::truthy(field(snapshot,"isUsingOverage"));
    const bool stashedMC=value::truthy(field(snapshot,"cachedMCEnabled"));
    const auto strategy=value(snapshot,"globalCacheStrategy");
    const auto effort=value(snapshot,"effortValue");
    const auto extraBodyHash=snapshot.contains("extraBodyParams")?
      computeHash(snapshot["extraBodyParams"]):0;
    auto betas=arrayStrings(field(snapshot,"betas"));
    std::sort(betas.begin(),betas.end());
    auto found=states_.find(*key);
    if(found==states_.end()) {
      while(states_.size()>=10&&!insertionOrder_.empty())
        erase(insertionOrder_.front());
      PreviousState state;
      state.systemHash=systemHash;state.gadgetsHash=gadgetsHash;
      state.stashControlHash=stashControlHash;
      state.gadgetNames=gadgetNames;
      state.perGadgetHashes=perGadgetHashes(strippedGadgets,gadgetNames);
      state.systemCharCount=count;state.model=model;state.fastMode=fastMode;
      state.globalStashStrategy=strategy;state.betas=betas;
      state.autoModeActive=autoMode;state.isUsingOverage=overage;
      state.stashedMCEnabled=stashedMC;state.effortValue=effort;
      state.extraBodyHash=extraBodyHash;state.callCount=1;
      state.systemForDiff=system;state.gadgetsForDiff=gadgetSchemas;
      states_[*key]=std::make_shared<PreviousState>(std::move(state));
      insertionOrder_.push_back(*key);
      return;
    }
    auto& previous=*found->second;
    ++previous.callCount;
    PendingChanges changes;
    const auto flag=[&](const char* name,bool condition){changes.flags[name]=condition;return condition;};
    const bool systemChanged=flag("systemPromptChanged",systemHash!=previous.systemHash);
    const bool gadgetsChanged=flag("toolSchemasChanged",gadgetsHash!=previous.gadgetsHash);
    const bool modelChanged=flag("modelChanged",model!=previous.model);
    const bool fastChanged=flag("fastModeChanged",fastMode!=previous.fastMode);
    const bool controlChanged=flag("cacheControlChanged",stashControlHash!=previous.stashControlHash);
    const bool strategyChanged=flag("globalCacheStrategyChanged",strategy!=previous.globalStashStrategy);
    const bool betasChanged=flag("betasChanged",betas!=previous.betas);
    const bool autoChanged=flag("autoModeChanged",autoMode!=previous.autoModeActive);
    const bool overageChanged=flag("overageChanged",overage!=previous.isUsingOverage);
    const bool stashedChanged=flag("cachedMCChanged",stashedMC!=previous.stashedMCEnabled);
    const bool effortChanged=flag("effortChanged",effort!=previous.effortValue);
    const bool extraChanged=flag("extraBodyChanged",extraBodyHash!=previous.extraBodyHash);
    if(systemChanged||gadgetsChanged||modelChanged||fastChanged||controlChanged||
        strategyChanged||betasChanged||autoChanged||overageChanged||
        stashedChanged||effortChanged||extraChanged) {
      const std::set<std::string> oldGadgets(previous.gadgetNames.begin(),previous.gadgetNames.end());
      const std::set<std::string> newGadgets(gadgetNames.begin(),gadgetNames.end());
      const std::set<std::string> oldBetas(previous.betas.begin(),previous.betas.end());
      const std::set<std::string> newBetas(betas.begin(),betas.end());
      changes.addedGadgets=difference(gadgetNames,oldGadgets);
      changes.removedGadgets=difference(previous.gadgetNames,newGadgets);
      changes.addedGadgetCount=static_cast<int>(changes.addedGadgets.size());
      changes.removedGadgetCount=static_cast<int>(changes.removedGadgets.size());
      if(gadgetsChanged) {
        const auto newHashes=perGadgetHashes(strippedGadgets,gadgetNames);
        for(const auto& name:gadgetNames)
          if(oldGadgets.contains(name)&&newHashes.at(name)!=previous.perGadgetHashes[name])
            changes.changedGadgetSchemas.push_back(name);
        previous.perGadgetHashes=newHashes;
      }
      changes.systemCharDelta=count-previous.systemCharCount;
      changes.previousModel=previous.model;changes.newModel=model;
      changes.prevGlobalStashStrategy=previous.globalStashStrategy;
      changes.newGlobalStashStrategy=strategy;
      changes.addedBetas=difference(betas,oldBetas);
      changes.removedBetas=difference(previous.betas,newBetas);
      changes.prevEffortValue=previous.effortValue;changes.newEffortValue=effort;
      changes.prevSystemForDiff=previous.systemForDiff;
      changes.prevGadgetsForDiff=previous.gadgetsForDiff;
      changes.hasPreviousDiffableContent=true;
      previous.pendingChanges=std::move(changes);
    } else previous.pendingChanges.reset();

    previous.systemHash=systemHash;previous.gadgetsHash=gadgetsHash;
    previous.stashControlHash=stashControlHash;previous.gadgetNames=gadgetNames;
    previous.systemCharCount=count;previous.model=model;
    previous.fastMode=fastMode;previous.globalStashStrategy=strategy;
    previous.betas=betas;previous.autoModeActive=autoMode;
    previous.isUsingOverage=overage;previous.stashedMCEnabled=stashedMC;
    previous.effortValue=effort;previous.extraBodyHash=extraBodyHash;
    previous.systemForDiff=system;previous.gadgetsForDiff=gadgetSchemas;
  }catch(const std::exception& error){services_.logError(error.what());}
}

void Detector::notifyStashDeletion(const std::string& querySource,
    const std::optional<std::string>& batId){
  auto key=trackingKey(querySource,batId);
  if(key&&states_.contains(*key))states_[*key]->stashDeletionsPending=true;
}
void Detector::notifyCompaction(const std::string& querySource,
    const std::optional<std::string>& batId){
  auto key=trackingKey(querySource,batId);
  if(key&&states_.contains(*key))states_[*key]->prevStashReadTokens.reset();
}
void Detector::cleanupBatTracking(const std::string& batId){erase(batId);}
void Detector::resetPoetryStashBreakDetection(){states_.clear();insertionOrder_.clear();}

} // namespace batbox::model_stash
