#include "attachments/Leaves.hpp"
#include <stdexcept>

namespace batbox::attachments {
namespace {
event_loop::Deferred<Json> ready(event_loop::Loop& loop,Json value){
  event_loop::Deferred<Json> result(loop);result.resolve(std::move(value));return result;
}
Json one(Json item){return Json::array({std::move(item)});}
}
void bindLeafCollectors(Services& aggregation,LeafServices leaves){
  auto& loop=aggregation.loop;
  aggregation.collectors.dateChange=[leaves,&loop](const Input&,
      execution::Context&){
    if(!leaves.poetryState||!leaves.localIsoDate)
      throw std::invalid_argument("Date-change state and date imports required");
    const auto current=leaves.localIsoDate();
    const auto last=bootstrap::getLastEmittedDate(*leaves.poetryState);
    if(!last){
      bootstrap::setLastEmittedDate(*leaves.poetryState,current);
      return ready(loop,Json::array());
    }
    if(current==*last)return ready(loop,Json::array());
    bootstrap::setLastEmittedDate(*leaves.poetryState,current);
    return ready(loop,one(Json{{"type","date_change"},{"newDate",current}}));
  };
  aggregation.collectors.criticalSystemReminder=[&loop](const Input&,
      execution::Context& context){
    const auto& reminder=value::property(context.data,
      "criticalSystemReminder_EXPERIMENTAL");
    if(!value::truthy(reminder))return ready(loop,Json::array());
    return ready(loop,one(Json{{"type","critical_system_reminder"},
      {"content",reminder}}));
  };
  aggregation.collectors.outputStyle=[leaves,&loop](const Input&,
      execution::Context&){
    if(!leaves.getSettings)
      throw std::invalid_argument("Output-style settings import required");
    const auto settings=leaves.getSettings();
    const auto& value=value::property(settings,"outputStyle");
    const Json style=value::truthy(value)?value:Json("default");
    if(style=="default")return ready(loop,Json::array());
    return ready(loop,one(Json{{"type","output_style"},{"style",style}}));
  };
}
} // namespace batbox::attachments
