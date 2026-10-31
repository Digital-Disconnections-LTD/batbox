#pragma once
#include "application/Config.hpp"
#include "application_model/LocalModelService.hpp"
#include "bootstrap/InvokedSkills.hpp"
#include "event_loop/Deferred.hpp"
#include "messages/Messages.hpp"
#include "skills/SkillRoster.hpp"
#include "skills/SkillGadgetValidation.hpp"
#include "skills/process/PoetryCommand.hpp"
#include "gadget_results/Storage.hpp"
#include "gadgets/Gadget.hpp"
#include "user_input/SlashDispatch.hpp"
#include <filesystem>
#include <map>
#include <memory>

namespace batbox::bats {struct BatContext;struct RunBatParams;}
namespace batbox::application_hooks {class Provider;}
namespace batbox::application_media {class AggregateProvider;}
namespace batbox::application_flights {class Flight;}
namespace batbox::gadgets::live_roster {struct Snapshot;}

namespace batbox::application_skills {
struct Inputs {
  event_loop::Loop& owner;
  application::Config config;
  std::filesystem::path projectRoot;
  std::shared_ptr<messages::Services> messages;
  std::shared_ptr<application_flights::Flight> flight;
  std::shared_ptr<application_model::LocalModelService> model;
  std::shared_ptr<bootstrap::InvokedSkillsState> invoked;
  std::function<Json()> currentMessages,mcpCommands,appState;
  std::function<std::shared_ptr<bats::BatContext>()> currentBat;
  std::function<std::shared_ptr<const gadgets::live_roster::Snapshot>()>
    currentGadgetSnapshot;
  gadget_results::StorageImports resultStorage;
  std::function<void(const std::string&)> diagnostic;
  std::function<void()> changed;
  explicit Inputs(event_loop::Loop& loop):owner(loop){}
};

class Provider final:public std::enable_shared_from_this<Provider>{
public:
  static std::shared_ptr<Provider> create(Inputs inputs);
  std::shared_ptr<skills::SkillRoster> roster()const{return roster_;}
  skills::SkillRosterServices rosterServices()const{return services_;}
  void bindRuntime(std::shared_ptr<application_media::AggregateProvider> attachments,
    std::shared_ptr<application_hooks::Provider> hooks);
  void bindBatRunner(std::function<void(const bats::RunBatParams&,
    const query::Yield&)> run);
  void bindMcpPoetryRunner(std::function<Json(const Json&,
    const std::string&,const bats::BatContext&)> run);
  Json modelCommands();
  std::vector<user_input::CommandEntry> slashCommands();
  gadgets::Gadget makeGadget();
  void clearStashes();
  Json processPoetry(const std::string& name,const std::string& args,
    const bats::BatContext& context,const Json& preceding=Json::array(),
    const Json& images=Json::array(),
    const std::optional<std::string>& uuid={});
  Json poetryForCommand(const Json& command,const std::string& args,
    const bats::BatContext& context){return poetryFor(command,args,context);}
  std::vector<Json> allCommands();
  Json findCommand(const std::string& name,const std::vector<Json>& all)const;
private:
  explicit Provider(Inputs inputs):inputs_(std::move(inputs)){}
  void initialize();
  std::vector<skills::SkillWithPath> loadLegacy(const std::string& cwd);
  std::vector<skills::SkillCommand> loadedSkills();
  Json descriptor(const skills::SkillCommand& command)const;
  Json poetryFor(const Json& descriptor,const std::string& args,
    const bats::BatContext& context);
  std::string executePoetryShell(const std::string& text,
    const std::string& slashCommandName,const std::optional<std::string>& shell,
    const std::vector<std::string>& allowedGadgets,
    const bats::BatContext& context);
  skills::process::PoetryCommandServices poetryServices(
    const bats::BatContext& context);
  skills::RuleEntries rules(const Json& context,const std::string& behavior)const;
  Inputs inputs_;
  std::shared_ptr<skills::SkillRoster> roster_;
  skills::SkillRosterServices services_;
  std::map<std::string,skills::SkillCommand> commandIndex_;
  std::shared_ptr<application_media::AggregateProvider> attachments_;
  std::shared_ptr<application_hooks::Provider> hooks_;
  std::function<void(const bats::RunBatParams&,const query::Yield&)> runBat_;
  std::function<Json(const Json&,const std::string&,
    const bats::BatContext&)> mcpPoetry_;
};
}
