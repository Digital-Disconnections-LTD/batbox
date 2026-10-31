#include "compaction/Compaction.hpp"

namespace batbox::compaction {
bool shouldAutoCompact(const Json& messages,const std::string& model,
                       const std::optional<std::string>& source,double freed,AutoServices& services) {
  if (source=="session_memory" || source=="compact") return false;
  if (services.feature("CONTEXT_COLLAPSE") && source=="marble_origami") return false;
  if (!services.isAutoCompactEnabled()) return false;
  if (services.feature("REACTIVE_COMPACT") && services.reactiveOnly()) return false;
  if (services.feature("CONTEXT_COLLAPSE") && services.isContextCollapseEnabled()) return false;
  const auto count=services.tokenCountWithEstimation(messages)-freed;
  // Threshold and window lookups below serve compaction decisions only.
  return services.calculateTokenWarningState(count,model).at("isAboveAutoCompactThreshold").get<bool>();
}
Json autoCompactIfNeeded(const Json& messages,const Json& context,const Json& stash,
                         const std::optional<std::string>& source,const Json& tracking,
                         double freed,AutoServices& services) {
  const auto disabled=services.environment.find("BATBOX_DISABLE_COMPACT");
  if (disabled!=services.environment.end() && isEnvTruthy(disabled->second)) return {{"wasCompacted",false}};
  const auto& failures=value::property(tracking,"consecutiveFailures");
  if (!failures.is_null() && failures.get<double>()>=3) return {{"wasCompacted",false}};
  const auto model=context.at("options").at("mainLoopModel").get<std::string>();
  if (!shouldAutoCompact(messages,model,source,freed,services)) return {{"wasCompacted",false}};
  Json info={{"isRecompactionInChain",value::property(tracking,"compacted")==true},
    {"turnsSincePreviousCompact",value::numberOr(value::property(tracking,"turnCounter"),-1)},
    {"autoCompactThreshold",services.getAutoCompactThreshold(model)}};
  if (tracking.contains("turnId")) info["previousCompactTurnId"]=tracking["turnId"];
  if (source) info["querySource"]=*source;
  auto memory=services.tryFlightMemoryCompaction(messages,value::property(context,"agentId"),
    info.at("autoCompactThreshold").get<double>());
  if (value::truthy(memory)) {
    services.clearLastSummarizedMessageId();
    services.runPostCompactCleanup(source);
      services.markPostCompaction();
    return {{"wasCompacted",true},{"compactionResult",memory}};
  }
  try {
    auto result=services.compactConversation(messages,context,stash,true,std::nullopt,true,info);
    services.clearLastSummarizedMessageId();
    services.runPostCompactCleanup(source);
    return {{"wasCompacted",true},{"compactionResult",result},{"consecutiveFailures",0}};
  } catch (...) {
    return {{"wasCompacted",false},{"consecutiveFailures",value::numberOr(failures)+1}};
  }
}
} // namespace batbox::compaction
