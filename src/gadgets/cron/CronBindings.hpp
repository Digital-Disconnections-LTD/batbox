#pragma once
#include "gadgets/cron/CronGadgets.hpp"
#include "bat_context/PartnerIdentity.hpp"
#include "cron/config/CronConfiguration.hpp"
#include "format/Truncate.hpp"
namespace batbox::gadgets::cron {
// The application supplies terminal renderers and owns loop and identity state.
// All remaining services use the Indigenous implementations.
CronGadgetServices composeIndigenousCronGadgetServices(event_loop::Loop& loop,
  std::shared_ptr<bootstrap::BootstrapState> state,
  batbox::cron::CronMissionServices missionServices,
  batbox::cron::config::Services configuration,
  bat_context::PartnerIdentityState& identity,
  format::TruncateServices truncation,CronGadgetUI ui);
}
