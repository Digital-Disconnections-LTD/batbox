#pragma once
#include "bats/RunBat.hpp"
#include "skills/SkillGadgetValidation.hpp"
#include "skills/remote/RemoteSkill.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::skills {
std::optional<std::string> getSkillGadgetUseIDFromParentMessage(
  const Json& parentMessage,const std::string& gadgetName="Skill");
std::vector<Json> tagSkillMessagesWithGadgetUseID(
  const std::vector<Json>& messages,const std::optional<std::string>& gadgetUseID);
std::vector<Json> filterSkillProcessedMessages(const std::vector<Json>& messages,
  const std::string& commandMessageTag="command-message");
Json mapSkillGadgetResult(const Json& result,const std::string& gadgetUseID);

struct SkillContextModifier {
  std::vector<std::string> allowedGadgets;
  std::optional<std::string> model;
  Json effort=nullptr;
  bool effortPresent=false;
  std::function<bool(const std::string&)> has1mContext;
  std::function<std::string(const std::string&)> parseUserSpecifiedModel;
  std::function<bool(const std::string&)> modelSupports1M;
};
std::string resolveSkillModelOverride(const std::string& skillModel,
  const std::string& currentModel,const SkillContextModifier& services);
Json applySkillAppStateModifier(const Json& appState,
  const SkillContextModifier& modifier);
Json applySkillOptionsModifier(const Json& context,
  const SkillContextModifier& modifier);

struct SkillExecutionServices {
  std::function<std::vector<Json>(const bats::BatContext&)> getAllCommands;
  SkillGadgetLookup lookup;
  std::function<Json(const std::string&,const std::string&,
    const std::vector<Json>&,const bats::BatContext&)> processPoetrySlashCommand;
  bats::CommandServices forkedCommand;
  std::function<std::string()> createBatId;
  std::function<void(const bats::RunBatParams&,const query::Yield&)> runBat;
  std::function<Json(const Json&)> normalizeMessages;
  std::function<void(const std::string&)> clearInvokedSkillsForBat;
  std::function<void(const std::string&)> recordSkillUsage;
  remote::Services remote;
  std::function<bool(const std::string&)> has1mContext;
  std::function<std::string(const std::string&)> parseUserSpecifiedModel;
  std::function<bool(const std::string&)> modelSupports1M;
  std::string commandMessageTag="command-message";
};
gadgets::GadgetResult executeSkill(const Json& input,
  const bats::BatContext& context,
  const std::function<std::future<Json>(const Json&)>& canUseGadget,
  const Json& parentMessage,const std::function<void(const gadgets::GadgetProgress&)>& onProgress,
  SkillExecutionServices& services,std::stop_token stopToken={});
}
