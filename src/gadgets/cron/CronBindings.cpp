#include "gadgets/cron/CronBindings.hpp"
#include <memory>
#include <utility>
namespace batbox::gadgets::cron {
CronGadgetServices composeIndigenousCronGadgetServices(event_loop::Loop& loop,
    std::shared_ptr<bootstrap::BootstrapState> state,
    batbox::cron::CronMissionServices missionServices,
    batbox::cron::config::Services configuration,
    bat_context::PartnerIdentityState& identity,
    format::TruncateServices truncation,CronGadgetUI ui){
  const auto gates=std::make_shared<batbox::cron::config::Services>(
    std::move(configuration));
  return makeIndigenousCronGadgetServices(loop,std::move(state),
    std::move(missionServices),
    [gates]{return batbox::cron::config::isCronEnabled(*gates);},
    [gates]{return batbox::cron::config::isDurableCronEnabled(*gates);},
    [&identity]{return bat_context::getBatId(identity);},
    format::bindTruncate(std::move(truncation)),std::move(ui));
}
}
