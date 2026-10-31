#include "bats/BatResultBindings.hpp"
#include <cassert>
using namespace batbox;
int main(){
  auto imports=bats::indigenousBatResultImports();
  const auto now=imports.nowMillis();assert(now>0);
  imports.nowMillis=[]{return 1000;};
  const Json usage{{"input_tokens",10},{"output_tokens",4},
    {"cache_creation_input_tokens",2},{"cache_read_input_tokens",3}};
  const Json first{{"type","assistant"},{"message",{
    {"content",Json::array({Json{{"type","text"},{"text","Earlier answer"}},
      Json{{"type","tool_use"},{"name","Bash"}}})},{"usage",usage}}}};
  const Json last{{"type","assistant"},{"message",{
    {"content",Json::array({Json{{"type","tool_use"},{"name","Read"}}})},
    {"usage",usage}}}};
  const Json messages=Json::array({first,last});
  auto partial=bats::extractPartialResult(messages,imports);
  assert(partial&&*partial=="Earlier answer");
  bats::BatResultMetadata metadata;metadata.startTime=900;
  metadata.batType="general";
  auto result=bats::finalizeBatGadget(messages,"worker-1",metadata,imports);
  assert(result.at("totalDurationMs")==100);
  assert(result.at("totalTokens")==19);
  assert(result.at("totalToolUseCount")==2);
  assert(result.at("content")==Json::array({Json{{"type","text"},{"text","Earlier answer"}}}));
}
