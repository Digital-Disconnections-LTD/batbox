#pragma once
#include "cron/CronMissions.hpp"
#include "gadgets/Gadget.hpp"
#include <future>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::gadgets::cron {
struct CronGadgetUI {
  std::function<Json(const Json&,const Json&,const Json&)> renderCreateResult;
  std::function<Json(const Json&,const Json&,const Json&)> renderDeleteResult;
  std::function<Json(const Json&,const Json&,const Json&)> renderListResult;
};
struct CronGadgetServices {
  std::function<bool()> cronEnabled,durableEnabled;
  std::function<double()> nowMs;
  std::function<std::optional<std::string>()> partnerBatId;
  std::function<std::string()> cronFilePath;
  std::function<std::string(const std::string&)> cronToHuman;
  std::function<std::string(const std::string&,double,bool)> truncate;
  std::function<std::future<std::vector<batbox::cron::CronMission>>()> listMissions;
  std::function<std::future<std::string>(const std::string&,
    const std::string&,bool,bool,const std::optional<std::string>&)> addMission;
  std::function<std::future<event_loop::Unit>(
    const std::vector<std::string>&)> removeMissions;
  std::function<void(bool)> setScheduledMissionsEnabled;
  CronGadgetUI ui;
};
Json parseCronCreateInput(const Json& input);
Json parseCronDeleteInput(const Json& input);
Json parseCronListInput(const Json& input);
Json cronInputSchema(const std::string& name);
Json cronOutputSchema(const std::string& name);
Gadget makeCronCreateGadget(CronGadgetServices services);
Gadget makeCronDeleteGadget(CronGadgetServices services);
Gadget makeCronListGadget(CronGadgetServices services);
CronGadgetServices makeIndigenousCronGadgetServices(event_loop::Loop& loop,
  std::shared_ptr<bootstrap::BootstrapState> state,
  batbox::cron::CronMissionServices missionServices,
  std::function<bool()> cronEnabled,
  std::function<bool()> durableEnabled,
  std::function<std::optional<std::string>()> partnerBatId,
  std::function<std::string(const std::string&,double,bool)> truncate,
  CronGadgetUI ui);
}
