#include "application/engine/Defaults.hpp"
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
void check(bool condition,const char* message="User context refresh check failed"){
  if(!condition)throw std::logic_error(message);
}
} // namespace

int main(){
  namespace engine=application::engine;
  const Json fresh={{"batboxMd","live project instructions"},
    {"currentDate","Today's date is 2026-11-21."},
    {"projectRecovery","recovery text"}};

  // A main-thread query adopts the live provider keys wholesale.
  execution::Context mainThread;
  const Json mainPrevious={{"batboxMd","stale instructions"},
    {"currentDate","stale date"},{"queryAddition","kept"}};
  const auto mainMerged=engine::mergeRefreshedUserContext(mainThread,mainPrevious,fresh);
  check(mainMerged.at("batboxMd")=="live project instructions","main adopts fresh batboxMd");
  check(mainMerged.at("currentDate")=="Today's date is 2026-11-21.","date refreshed");
  check(mainMerged.contains("projectRecovery"),"provider keys added");
  check(mainMerged.at("queryAddition")=="kept","query additions preserved");

  // A Bat whose resolved context never held batboxMd keeps it omitted.
  execution::Context batContext;
  batContext.data=Json{{"agentId","bat-1"}};
  const Json trimmed={{"currentDate","stale date"}};
  const auto batMerged=engine::mergeRefreshedUserContext(batContext,trimmed,fresh);
  check(!batMerged.contains("batboxMd"),"omitted batboxMd must not re-inflate");
  check(batMerged.at("currentDate")=="Today's date is 2026-11-21.","date still refreshed");
  check(batMerged.contains("projectRecovery"),"other provider keys still join");

  // A Bat that kept batboxMd refreshes it like the main thread does.
  const Json batWith={{"batboxMd","stale instructions"}};
  const auto kept=engine::mergeRefreshedUserContext(batContext,batWith,fresh);
  check(kept.at("batboxMd")=="live project instructions","kept batboxMd refreshes");

  // Removals apply: a provider key gone from the live context leaves too.
  const Json partial={{"currentDate","Today's date is 2026-11-21."}};
  const Json hadRecovery={{"batboxMd","stale"},{"projectRecovery","stale recovery"}};
  const auto shrunk=engine::mergeRefreshedUserContext(mainThread,hadRecovery,partial);
  check(!shrunk.contains("projectRecovery"),"provider removal applies");
  try{
    engine::mergeRefreshedUserContext(mainThread,mainPrevious,Json::array());
    check(false,"a non-object provider context must be rejected");
  }catch(const std::invalid_argument&){}
  std::cout<<"user context refresh indigenous checks passed\n";
  return 0;
}
