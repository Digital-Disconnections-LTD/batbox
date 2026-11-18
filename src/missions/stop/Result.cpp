#include "missions/stop/MissionStop.hpp"

namespace batbox::missions::stop {
Json mapGadgetResult(const Json& output,const std::string& id){
  return {{"tool_use_id",id},{"type","tool_result"},{"content",value::stringify(output)}};
}
} // namespace batbox::missions::stop
