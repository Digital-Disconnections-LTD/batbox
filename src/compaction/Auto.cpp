#include "compaction/Compaction.hpp"
#include "model/ModelWrappers.hpp"
#include <algorithm>

namespace batbox::compaction {
bool shouldAutoCompact(const Json& messages,const std::string& model,
                       const std::optional<std::string>& source,AutoServices& services) {
  if (source=="session_memory" || source=="compact") return false;
  if (!services.isAutoCompactEnabled()) return false;
  const auto count=services.tokenCountWithEstimation(messages);
  // Threshold and window lookups below serve compaction decisions only.
  return services.calculateTokenWarningState(count,model).at("isAboveAutoCompactThreshold").get<bool>();
}
Json autoCompactIfNeeded(const Json& messages,const Json& context,const Json& stash,
                         const std::optional<std::string>& source,const Json& tracking,
                         AutoServices& services) {
  const auto disabled=services.environment.find("BATBOX_DISABLE_COMPACT");
  if (disabled!=services.environment.end() && isEnvTruthy(disabled->second)) return {{"wasCompacted",false}};
  const auto& failures=value::property(tracking,"consecutiveFailures");
  if (!failures.is_null() && failures.get<double>()>=services.failureLimit) return {{"wasCompacted",false}};
  const auto model=context.at("options").at("mainLoopModel").get<std::string>();
  if (!shouldAutoCompact(messages,model,source,services)) return {{"wasCompacted",false}};
  Json info={{"isRecompactionInChain",value::property(tracking,"compacted")==true},
    {"turnsSincePreviousCompact",value::numberOr(value::property(tracking,"turnCounter"),-1)},
    {"autoCompactThreshold",services.getAutoCompactThreshold(model)}};
  if (tracking.contains("turnId")) info["previousCompactTurnId"]=tracking["turnId"];
  if (source) info["querySource"]=*source;
  try {
    auto result=services.compactConversation(messages,context,stash,true,std::nullopt,true,info);
    services.runPostCompactCleanup(source);
    return {{"wasCompacted",true},{"compactionResult",result},{"consecutiveFailures",0}};
  } catch (const CompactionTimeout& timeout) {
    // A slow server timing out is not a broken request: report it and leave
    // the next attempt to the next submission instead of retrying now.
    if (services.reportFailure) services.reportFailure(timeout.what(),true);
    return {{"wasCompacted",false},
      {"consecutiveFailures",std::max(value::numberOr(failures)+1,services.failureLimit)}};
  } catch (const model::UserAbortError&) {
    return {{"wasCompacted",false}};
  } catch (const std::exception& failure) {
    if (failure.what()==std::string(ERROR_USER_ABORT)) return {{"wasCompacted",false}};
    if (services.reportFailure) services.reportFailure(failure.what(),false);
    return {{"wasCompacted",false},{"consecutiveFailures",value::numberOr(failures)+1}};
  } catch (...) {
    if (services.reportFailure) services.reportFailure("unknown error",false);
    return {{"wasCompacted",false},{"consecutiveFailures",value::numberOr(failures)+1}};
  }
}
bool isTimeoutText(const std::string& text) {
  return text.find("timed out")!=std::string::npos||text.find("idle timeout")!=std::string::npos;
}
} // namespace batbox::compaction
