#include "terminal_hooks/TabStatus.hpp"
#include <stdexcept>

namespace batbox::terminal_hooks {
std::string tabStatusPreset(TabStatusKind kind,
    const std::function<std::string(const Json&)>& osc){
  if(!osc)throw std::logic_error("OSC import required");
  std::string fields;
  switch(kind){
    case TabStatusKind::Idle:
      fields="indicator=#00d75f;status=Idle;status-color=#888888";break;
    case TabStatusKind::Busy:
      fields="indicator=#ff9500;status=Working…;status-color=#ff9500";break;
    case TabStatusKind::Waiting:
      fields="indicator=#5f87ff;status=Waiting;status-color=#5f87ff";break;
  }
  return osc(Json::array({21337,fields}));
}
std::string clearTabStatus(
    const std::function<std::string(const Json&)>& osc){
  if(!osc)throw std::logic_error("OSC import required");
  return osc(Json::array({21337,"indicator=;status=;status-color="}));
}
void TabStatusEffect::commit(std::optional<TabStatusKind> kind,
    WriteRawRef writeRaw,const TabStatusServices& services){
  if(mounted_&&kind==previousDependency_&&writeRaw==previousWrite_)return;
  mounted_=true;
  previousDependency_=kind;
  previousWrite_=writeRaw;
  if(!kind){
    if(previousKindRef_&&writeRaw){
      if(!services.supportsTabStatus||!services.wrapForMultiplexer)
        throw std::logic_error("Tab status imports required");
      if(services.supportsTabStatus())
        (*writeRaw)(services.wrapForMultiplexer(clearTabStatus(services.osc)));
    }
    previousKindRef_.reset();
    return;
  }
  previousKindRef_=kind;
  if(!writeRaw)return;
  if(!services.supportsTabStatus||!services.wrapForMultiplexer)
    throw std::logic_error("Tab status imports required");
  if(!services.supportsTabStatus())return;
  (*writeRaw)(services.wrapForMultiplexer(
    tabStatusPreset(*kind,services.osc)));
}
}
