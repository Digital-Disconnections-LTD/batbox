#pragma once
#include "bats/ForkedBat.hpp"
#include "messages/Messages.hpp"
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace batbox::skills::process {
class MalformedCommandError:public std::runtime_error {
 public:explicit MalformedCommandError(const std::string& what):std::runtime_error(what){}
};
struct PoetryCommandServices {
  messages::Services messageServices;
  std::function<Json(const std::string&,const std::vector<Json>&)> findCommand;
  std::function<Json(const Json&,const std::string&,const bats::BatContext&)>
    getPoetryForCommand;
  std::function<bool(const std::string&)> feature;
  std::string coordinatorModeEnvironment;
  std::function<bool(const std::string&)> isRestrictedToPluginOnly;
  std::function<std::string()> getFlightId;
  std::function<void(const bats::BatContext&,const std::string&,
    const Json&,const std::string&,const std::optional<std::string>&)> registerSkillHooks;
  std::function<std::optional<std::string>()> currentBatId;
  std::function<void(const std::string&,const std::string&,
    const std::string&,const std::optional<std::string>&)> addInvokedSkill;
  std::function<Json(const std::string&,const bats::BatContext&,
    const Json&,const Json&,const std::string&,bool)> getAttachmentMessages;
  std::function<std::string()> randomUUID,nowISO;
};

bool isEnvTruthy(const std::string& value);
bool isSourceAdminTrusted(const Json& source);
std::vector<std::string> parseGadgetListFromCLI(const Json& gadgets);
std::string formatSkillLoadingMetadata(const std::string& name,
  const std::string& progressMessage="loading");
std::string formatSlashCommandLoadingMetadata(const std::string& name,
  const std::optional<std::string>& args=std::nullopt);
std::string formatCommandLoadingMetadata(const Json& command,
  const std::optional<std::string>& args=std::nullopt);
Json createAttachmentMessage(const Json& attachment,PoetryCommandServices& services);
Json processPoetrySlashCommand(const std::string& commandName,const std::string& args,
  const std::vector<Json>& commands,const bats::BatContext& context,
  PoetryCommandServices& services,const Json& imageContentBlocks=Json::array());
Json getMessagesForPoetrySlashCommand(const Json& command,const std::string& args,
  const bats::BatContext& context,PoetryCommandServices& services,
  const Json& precedingInputBlocks=Json::array(),
  const Json& imageContentBlocks=Json::array(),
  const std::optional<std::string>& uuid=std::nullopt);
}
