#include "gadgets/mission_stop/MissionStop.hpp"

namespace batbox::mission_stop {
Json mapGadgetResult(const Json& output,const std::string& id){
  return {{"tool_use_id",id},{"type","tool_result"},{"content",value::stringify(output)}};
}
} // namespace batbox::mission_stop
