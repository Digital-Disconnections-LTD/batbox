#include "settings/persistence/MdmPoll.hpp"
#include <stdexcept>
#include <utility>

namespace batbox::settings::persistence {
namespace {
std::string snapshot(const Json& mdm,const Json& hkcu){
  return value::stringify(Json{{"mdm",mdm},{"hkcu",hkcu}});
}
}
MdmPoll::MdmPoll(MdmPollServices services):services_(std::move(services)){
  if(!services_.getMdmSettings||!services_.getHkcuSettings||
      !services_.refreshMdmSettings||!services_.setMdmSettingsStash||
      !services_.setInterval||!services_.debug||
      !services_.fanOutPolicySettings)
    throw std::invalid_argument("Missing MDM poll binding");
}
void MdmPoll::start(){
  const auto initial=services_.getMdmSettings();
  const auto initialHkcu=services_.getHkcuSettings();
  lastSnapshot_=snapshot(initial.settings,initialHkcu.settings);
  interval_=services_.setInterval(services_.pollIntervalMs,
    [this](){tick();},true);
}
void MdmPoll::tick(){
  if(disposed_)return;
  services_.refreshMdmSettings([this](std::optional<MdmValues> current,
      std::optional<std::string> error){
    if(error){
      services_.debug("MDM poll error: "+*error);
      return;
    }
    if(disposed_||!current)return;
    const auto currentSnapshot=snapshot(current->mdm.settings,
      current->hkcu.settings);
    if(currentSnapshot!=lastSnapshot_){
      lastSnapshot_=currentSnapshot;
      services_.setMdmSettingsStash(*current);
      services_.debug("Detected MDM settings change via poll");
      services_.fanOutPolicySettings();
    }
  });
}
void MdmPoll::dispose(){
  disposed_=true;
  if(interval_){(*interval_)();interval_.reset();}
  lastSnapshot_.reset();
}
}
