#include "gadgets/cron/CronGadgets.hpp"
#include "cron/CronHuman.hpp"
#include "bootstrap/BootstrapState.hpp"
#include "runtime/Platform.hpp"
#include <future>
#include <memory>
#include <stdexcept>
#include <utility>

namespace batbox::gadgets::cron {
namespace {
template<class T,class Operation>std::future<T> ownerDeferred(
  event_loop::Loop& loop,Operation operation){
  auto promise=std::make_shared<std::promise<T>>();
  auto future=promise->get_future();
  auto begin=[promise,operation=std::move(operation)]()mutable{
    try{operation().then([promise](const T* value,
      const std::exception_ptr& error){
      if(error)promise->set_exception(error);
      else promise->set_value(*value);
    });}
    catch(...){promise->set_exception(std::current_exception());}
  };
  const auto owner=loop.dispatcher();
  try{if(owner.isOwnerThread())begin();
    else owner.postMission(std::move(begin));}
  catch(...){promise->set_exception(std::current_exception());}
  return future;
}
template<class T,class Operation>T ownerSync(event_loop::Loop& loop,
  Operation operation){
  if(loop.isOwnerThread())return operation();
  auto promise=std::make_shared<std::promise<T>>();
  auto result=promise->get_future();
  loop.dispatcher().postMission([promise,operation=std::move(operation)]()mutable{
    try{promise->set_value(operation());}
    catch(...){promise->set_exception(std::current_exception());}
  });
  return result.get();
}
}
CronGadgetServices makeIndigenousCronGadgetServices(event_loop::Loop& loop,
  std::shared_ptr<bootstrap::BootstrapState> state,
  batbox::cron::CronMissionServices missionServices,
  std::function<bool()> cronEnabled,
  std::function<bool()> durableEnabled,
  std::function<std::optional<std::string>()> partnerBatId,
  std::function<std::string(const std::string&,double,bool)> truncate,
  CronGadgetUI ui){
  if(!state||!cronEnabled||!durableEnabled||!partnerBatId||
     !truncate||!ui.renderCreateResult||!ui.renderDeleteResult||
     !ui.renderListResult)
    throw std::invalid_argument("Cron native tool imports required");
  auto missions=std::make_shared<batbox::cron::CronMissionServices>(
    std::move(missionServices));
  CronGadgetServices services;
  services.cronEnabled=[&loop,gate=std::move(cronEnabled)]{
    return ownerSync<bool>(loop,gate);
  };
  services.durableEnabled=[&loop,gate=std::move(durableEnabled)]{
    return ownerSync<bool>(loop,gate);
  };
  services.partnerBatId=std::move(partnerBatId);
  services.truncate=std::move(truncate);
  services.ui=std::move(ui);
  services.nowMs=[]{return static_cast<double>(platform::nowMillis());};
  services.cronFilePath=[&loop,missions]{
    return ownerSync<std::string>(loop,[missions]{
      return batbox::cron::getCronFilePath(std::nullopt,*missions);
    });
  };
  const auto human=batbox::cron::indigenousCronHumanServices();
  services.cronToHuman=[human](const std::string& expression){
    return batbox::cron::cronToHuman(expression,false,human);
  };
  services.listMissions=[&loop,missions]{
    return ownerDeferred<std::vector<batbox::cron::CronMission>>(loop,
      [&loop,missions]{return batbox::cron::listAllCronMissions(
        loop,std::nullopt,*missions);});
  };
  services.addMission=[&loop,missions](const std::string& expression,
    const std::string& poetry,bool recurring,bool durable,
    const std::optional<std::string>& batId){
    return ownerDeferred<std::string>(loop,[&loop,missions,expression,poetry,
      recurring,durable,batId]{
      return batbox::cron::addCronMission(loop,expression,poetry,
        recurring,durable,batId,*missions);
    });
  };
  services.removeMissions=[&loop,missions](const std::vector<std::string>& ids){
    return ownerDeferred<event_loop::Unit>(loop,[&loop,missions,ids]{
      return batbox::cron::removeCronMissions(loop,ids,std::nullopt,*missions);
    });
  };
  services.setScheduledMissionsEnabled=[&loop,state](bool enabled){
    (void)ownerSync<event_loop::Unit>(loop,[state,enabled]{
      bootstrap::setScheduledMissionsEnabled(state->flags,enabled);
      return event_loop::Unit{};
    });
  };
  return services;
}
}
