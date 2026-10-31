#include "model_stash/Detection.hpp"
#include <stdexcept>

namespace batbox::model_stash {
namespace {
std::string number(double n){return value::numberToString(n);}
std::string join(const std::vector<std::string>& items,const std::string& separator){
  std::string out;
  for(std::size_t i=0;i<items.size();++i){if(i)out+=separator;out+=items[i];}
  return out;
}
}

std::optional<PendingCheck> Detector::beginResponseCheck(const std::string& querySource,
    double stashReadTokens,double stashCreationTokens,const Json& messages,
    const std::optional<std::string>& batId,
    const std::optional<std::string>& /* requestId */){
  try {
    const auto key=trackingKey(querySource,batId);
    if(!key)return std::nullopt;
    const auto found=states_.find(*key);
    if(found==states_.end())return std::nullopt;
    auto& state=*found->second;
    if(state.model.find(services_.excludedModelFamily)!=std::string::npos)return std::nullopt;

    const auto previousRead=state.prevStashReadTokens;
    state.prevStashReadTokens=stashReadTokens;
    std::optional<double> timeSinceLastAssistant;
    if(messages.is_array()) {
      for(auto item=messages.rbegin();item!=messages.rend();++item){
        if(!item->is_object()||value::property(*item,"type")!="assistant")continue;
        const auto& timestamp=value::property(*item,"timestamp");
        if(timestamp.is_string())
          timeSinceLastAssistant=services_.nowMs()-
            services_.parseTimestampMs(timestamp.get<std::string>());
        break;
      }
    }
    if(!previousRead)return std::nullopt;
    const auto changes=state.pendingChanges;
    if(state.stashDeletionsPending){
      state.stashDeletionsPending=false;
      services_.logForDebugging("[PROMPT CACHE] cache deletion applied, cache read: "+
        number(*previousRead)+" → "+number(stashReadTokens)+" (expected drop)","debug");
      state.pendingChanges.reset();
      return std::nullopt;
    }
    const auto tokenDrop=*previousRead-stashReadTokens;
    if(stashReadTokens>=*previousRead*0.95||tokenDrop<2000){
      state.pendingChanges.reset();return std::nullopt;
    }

    std::vector<std::string> parts;
    if(changes){
      if(changes->flag("modelChanged"))
        parts.push_back("model changed ("+changes->previousModel+" → "+changes->newModel+")");
      if(changes->flag("systemPromptChanged")){
        const auto delta=changes->systemCharDelta;
        const auto info=delta==0?"":delta>0?
          " (+"+std::to_string(delta)+" chars)":
          " ("+std::to_string(delta)+" chars)";
        parts.push_back("system prompt changed"+info);
      }
      if(changes->flag("toolSchemasChanged")){
        const auto diff=changes->addedGadgetCount>0||changes->removedGadgetCount>0?
          " (+"+std::to_string(changes->addedGadgetCount)+"/-"+
            std::to_string(changes->removedGadgetCount)+" tools)":
          " (tool prompt/schema changed, same tool set)";
        parts.push_back("tools changed"+diff);
      }
      if(changes->flag("fastModeChanged"))parts.push_back("fast mode toggled");
      if(changes->flag("globalCacheStrategyChanged"))
        parts.push_back("global cache strategy changed ("+
          (changes->prevGlobalStashStrategy.empty()?"none":changes->prevGlobalStashStrategy)+
          " → "+(changes->newGlobalStashStrategy.empty()?"none":
            changes->newGlobalStashStrategy)+")");
      if(changes->flag("cacheControlChanged")&&
          !changes->flag("globalCacheStrategyChanged")&&
          !changes->flag("systemPromptChanged"))
        parts.push_back("cache_control changed (scope or TTL)");
      if(changes->flag("betasChanged")){
        const auto added=changes->addedBetas.empty()?"":"+"+join(changes->addedBetas,",");
        const auto removed=changes->removedBetas.empty()?"":"-"+join(changes->removedBetas,",");
        const auto diff=join(std::vector<std::string>{added,removed}," ");
        const auto compact=added.empty()?removed:removed.empty()?added:diff;
        parts.push_back("betas changed"+(compact.empty()?"":" ("+compact+")"));
      }
      if(changes->flag("autoModeChanged"))parts.push_back("auto mode toggled");
      if(changes->flag("overageChanged"))
        parts.push_back("overage state changed (TTL latched, no flip)");
      if(changes->flag("cachedMCChanged"))parts.push_back("cached microcompact toggled");
      if(changes->flag("effortChanged"))
        parts.push_back("effort changed ("+
          (changes->prevEffortValue.empty()?"default":changes->prevEffortValue)+
          " → "+(changes->newEffortValue.empty()?"default":changes->newEffortValue)+")");
      if(changes->flag("extraBodyChanged"))parts.push_back("extra body params changed");
    }
    const bool over5=timeSinceLastAssistant&&*timeSinceLastAssistant>5*60*1000;
    const bool over1h=timeSinceLastAssistant&&*timeSinceLastAssistant>60*60*1000;
    std::string reason;
    if(!parts.empty())reason=join(parts,", ");
    else if(over1h)reason="possible 1h TTL expiry (prompt unchanged)";
    else if(over5)reason="possible 5min TTL expiry (prompt unchanged)";
    else if(timeSinceLastAssistant)
      reason="likely server-side (prompt unchanged, <5min gap)";
    else reason="unknown cause";

    // Diagnostic details stay local; no sampled analytics payload is sent.
    PendingCheck pending;
    pending.state=found->second;
    pending.querySource=querySource;
    pending.reason=std::move(reason);
    pending.previousRead=*previousRead;
    pending.stashReadTokens=stashReadTokens;
    pending.stashCreationTokens=stashCreationTokens;
    if(changes&&changes->hasPreviousDiffableContent){
      pending.previousContent=buildDiffableContent(changes->prevSystemForDiff,
        changes->prevGadgetsForDiff,changes->previousModel);
      pending.currentContent=buildDiffableContent(state.systemForDiff,
        state.gadgetsForDiff,state.model);
      return pending;
    }
    finishResponseCheck(std::move(pending),std::nullopt);
    return std::nullopt;
  }catch(const std::exception& error){services_.logError(error.what());return std::nullopt;}
}

void Detector::finishResponseCheck(PendingCheck pending,
    const std::optional<std::string>& diffPath){
  try{
    const auto suffix=diffPath&& !diffPath->empty()?", diff: "+*diffPath:"";
    const auto summary="[PROMPT CACHE BREAK] "+pending.reason+
      " [source="+pending.querySource+", call #"+
      std::to_string(pending.state->callCount)+", cache read: "+
      number(pending.previousRead)+" → "+number(pending.stashReadTokens)+
      ", creation: "+number(pending.stashCreationTokens)+suffix+"]";
    services_.logForDebugging(summary,"warn");
    pending.state->pendingChanges.reset();
  }catch(const std::exception& error){services_.logError(error.what());}
}

void Detector::checkResponseForStashBreak(const std::string& querySource,
    double stashReadTokens,double stashCreationTokens,const Json& messages,
    const std::optional<std::string>& batId,
    const std::optional<std::string>& requestId){
  auto pending=beginResponseCheck(querySource,stashReadTokens,
    stashCreationTokens,messages,batId,requestId);
  if(!pending)return;
  try{
    if(!services_.writeStashBreakDiff)
      throw std::invalid_argument("Local cache-break diff writer import is required");
    auto path=services_.writeStashBreakDiff(pending->previousContent,
      pending->currentContent);
    finishResponseCheck(std::move(*pending),path);
  }catch(const std::exception& error){services_.logError(error.what());}
}

} // namespace batbox::model_stash
