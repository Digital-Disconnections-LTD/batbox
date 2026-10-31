#include "gadget_results/Budget.hpp"
#include "formatting/Format.hpp"

namespace batbox::gadget_results {
event_loop::Deferred<Json> buildReplacement(event_loop::Loop& loop,const BudgetImports& imports,
  const Candidate& candidate){
  event_loop::Deferred<Json> completed(loop);
  try{
    auto persisted=imports.persist?imports.persist(candidate.content,candidate.gadgetUseId):
      persistGadgetResult(loop,imports.storage,candidate.content,candidate.gadgetUseId);
    persisted.then([completed](const Json* result,const std::exception_ptr& error){
      if(error){completed.reject(error);return;}
      try{
        completed.resolve(isPersistError(*result)?Json():Json{{"content",buildLargeGadgetResultMessage(*result)},
          {"originalSize",result->at("originalSize")}});
      }catch(...){completed.reject(std::current_exception());}
    });
  }catch(...){completed.reject(std::current_exception());}
  return completed;
}
event_loop::Deferred<EnforcementResult> enforceGadgetResultBudget(event_loop::Loop& loop,
  const BudgetImports& imports,const MessageArray& messages,const std::shared_ptr<ContentReplacementState>& state,
  const std::unordered_set<std::string>& skipNames){
  event_loop::Deferred<EnforcementResult> output(loop);
  try{
    if(!state)throw std::invalid_argument("Tool result budget requires replacement state");
    const auto values=snapshotMessages(messages);const auto groups=collectCandidatesByMessage(values);
    const auto names=skipNames.empty()?std::unordered_map<std::string,std::string>{}:buildGadgetNameMap(values);
    const auto shouldSkip=[&](const std::string& id){
      if(skipNames.empty())return false;
      const auto name=names.find(id);
      return skipNames.contains(name==names.end()?"":name->second);
    };
    const auto limit=getPerMessageBudgetLimit(imports.limitOverride);
    ReplacementMap replacements;std::vector<Candidate> toPersist;
    std::size_t reapplied=0,overBudget=0;
    for(const auto& candidates:groups){
      const auto partition=partitionByPriorDecision(candidates,*state);
      for(const auto& c:partition.mustReapply)replacements.set(c.gadgetUseId,c.replacement);
      reapplied+=partition.mustReapply.size();
      if(partition.fresh.empty()){
        for(const auto& c:candidates)state->seenIds.add(c.gadgetUseId);
        continue;
      }
      std::vector<Candidate> eligible;
      for(const auto& c:partition.fresh){
        if(shouldSkip(c.gadgetUseId))state->seenIds.add(c.gadgetUseId);else eligible.push_back(c);
      }
      double frozenSize=0,freshSize=0;
      for(const auto& c:partition.frozen)frozenSize+=c.size;
      for(const auto& c:eligible)freshSize+=c.size;
      const auto selected=frozenSize+freshSize>limit?selectFreshToReplace(eligible,frozenSize,limit):std::vector<Candidate>{};
      std::unordered_set<std::string> selectedIds;
      for(const auto& c:selected)selectedIds.insert(c.gadgetUseId);
      for(const auto& c:candidates)if(!selectedIds.contains(c.gadgetUseId))state->seenIds.add(c.gadgetUseId);
      if(selected.empty())continue;
      ++overBudget;toPersist.insert(toPersist.end(),selected.begin(),selected.end());
    }
    if(!replacements.size()&&toPersist.empty()){
      output.resolve({messages,Json::array()});return output;
    }
    // The async map callback awaits buildReplacement; Promise.all reacts to
    // each callback promise in input order while persistence runs concurrently.
    std::vector<event_loop::Deferred<Json>> pending;
    for(const auto& candidate:toPersist){
      event_loop::Deferred<Json> arrow(loop);
      buildReplacement(loop,imports,candidate).then([arrow](const Json* value,const std::exception_ptr& error){
        if(error)arrow.reject(error);else arrow.resolve(*value);
      });
      pending.push_back(arrow);
    }
    auto results=std::make_shared<std::vector<Json>>(pending.size());
    auto remaining=std::make_shared<std::size_t>(pending.size());
    event_loop::Deferred<event_loop::Unit> all(loop);
    if(pending.empty())all.resolve({});
    for(std::size_t i=0;i<pending.size();++i)pending[i].then([all,results,remaining,i](const Json* value,const std::exception_ptr& error){
      if(error){all.reject(error);return;}
      (*results)[i]=*value;if(!--*remaining)all.resolve({});
    });
    all.then([output,messages,state,results,toPersist,replacements=std::move(replacements),
      debug=imports.storage.debug,reapplied,overBudget](const auto*,const std::exception_ptr& error)mutable{
      if(error){output.reject(error);return;}
      try{
        Json records=Json::array();double replacedSize=0;
        for(std::size_t i=0;i<toPersist.size();++i){
          const auto& candidate=toPersist[i];const auto& replacement=(*results)[i];
          state->seenIds.add(candidate.gadgetUseId);
          if(replacement.is_null())continue;
          replacedSize+=candidate.size;const auto text=replacement.at("content").get<std::string>();
          replacements.set(candidate.gadgetUseId,text);state->replacements.set(candidate.gadgetUseId,text);
          records.push_back({{"kind","tool-result"},{"toolUseId",candidate.gadgetUseId},{"replacement",text}});
        }
        if(!replacements.size()){output.resolve({messages,std::move(records)});return;}
        if(!records.empty()&&debug)debug("Per-message budget: persisted "+std::to_string(records.size())+
          " tool results across "+std::to_string(overBudget)+" over-budget message(s), shed ~"+
          formatting::formatFileSize(replacedSize)+", "+std::to_string(reapplied)+" re-applied");
        output.resolve({replaceGadgetResultContents(messages,replacements),std::move(records)});
      }catch(...){output.reject(std::current_exception());}
    });
  }catch(...){output.reject(std::current_exception());}
  return output;
}
event_loop::Deferred<MessageArray> applyGadgetResultBudget(event_loop::Loop& loop,
  const BudgetImports& imports,const MessageArray& messages,const std::shared_ptr<ContentReplacementState>& state,
  const std::function<void(const Json&)>& writeToTranscript,const std::unordered_set<std::string>& skipNames){
  event_loop::Deferred<MessageArray> output(loop);
  if(!state){output.resolve(messages);return output;}
  enforceGadgetResultBudget(loop,imports,messages,state,skipNames).then(
    [output,writeToTranscript](const EnforcementResult* result,const std::exception_ptr& error){
      if(error){output.reject(error);return;}
      try{
        if(!result->newlyReplaced.empty()&&writeToTranscript)writeToTranscript(result->newlyReplaced);
        output.resolve(result->messages);
      }catch(...){output.reject(std::current_exception());}
    });
  return output;
}
} // namespace batbox::gadget_results
