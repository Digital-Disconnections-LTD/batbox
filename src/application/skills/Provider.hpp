#pragma once
#include "application/Config.hpp"
#include "application/model/LocalModelService.hpp"
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
namespace batbox::application::hooks {class Provider;}
namespace batbox::application::media {class AggregateProvider;}
namespace batbox::application::flights {class Flight;}
namespace batbox::gadgets::live_roster {struct Snapshot;}

namespace batbox::application::skills {
struct Inputs {
  event_loop::Loop& owner;
  application::Config config;
  std::filesystem::path projectRoot;
  std::shared_ptr<messages::Services> messages;
  std::shared_ptr<application::flights::Flight> flight;
  std::shared_ptr<application::model::LocalModelService> model;
  std::shared_ptr<bootstrap::InvokedSkillsState> invoked;
  std::function<Json()> currentMessages,mcpCommands,appState;
  std::function<std::shared_ptr<batbox::bats::BatContext>()> currentBat;
  std::function<std::shared_ptr<const batbox::gadgets::live_roster::Snapshot>()>
    currentGadgetSnapshot;
  gadget_results::StorageImports resultStorage;
  std::function<void(const std::string&)> diagnostic;
  std::function<void()> changed;
  explicit Inputs(event_loop::Loop& loop):owner(loop){}
};

class Provider final:public std::enable_shared_from_this<Provider>{
public:
  static std::shared_ptr<Provider> create(Inputs inputs);
  std::shared_ptr<batbox::skills::SkillRoster> roster()const{return roster_;}
  batbox::skills::SkillRosterServices rosterServices()const{return services_;}
  void bindRuntime(std::shared_ptr<application::media::AggregateProvider> attachments,
    std::shared_ptr<application::hooks::Provider> hooks);
  void bindBatRunner(std::function<void(const batbox::bats::RunBatParams&,
    const query::Yield&)> run);
  void bindMcpPoetryRunner(std::function<Json(const Json&,
    const std::string&,const batbox::bats::BatContext&)> run);
  // Fills in a Skill Bat's run from a fresh Flight context and permission check.
  using ForkedRun=std::function<void(const batbox::bats::BatContext&,
    const std::function<std::future<Json>(const Json&)>&,batbox::bats::RunBatParams&)>;
  // Starts a `context: fork` Skill typed as /name as a background Bat, under
  // the given title; settles once it is under way.
  using SkillBatLauncher=std::function<event_loop::Deferred<event_loop::Unit>(
    std::string title,ForkedRun prepare)>;
  void bindSkillBatLauncher(SkillBatLauncher launch);
  Json modelCommands();
  std::vector<user_input::CommandEntry> slashCommands();
  batbox::gadgets::Gadget makeGadget();
  void clearStashes();
  Json processPoetry(const std::string& name,const std::string& args,
    const batbox::bats::BatContext& context,const Json& preceding=Json::array(),
    const Json& images=Json::array(),
    const std::optional<std::string>& uuid={});
  Json poetryForCommand(const Json& command,const std::string& args,
    const batbox::bats::BatContext& context){return poetryFor(command,args,context);}
  std::vector<Json> allCommands();
  Json findCommand(const std::string& name,const std::vector<Json>& all)const;
private:
  explicit Provider(Inputs inputs):inputs_(std::move(inputs)){}
  void initialize();
  std::vector<batbox::skills::SkillWithPath> loadLegacy(const std::string& cwd);
  std::vector<batbox::skills::SkillCommand> loadedSkills();
  Json descriptor(const batbox::skills::SkillCommand& command)const;
  Json poetryFor(const Json& descriptor,const std::string& args,
    const batbox::bats::BatContext& context);
  // The run the Skill Gadget gives a forked Skill, for the same Skill typed
  // as /name. Worker thread.
  void prepareForkedRun(const Json& descriptor,const std::string& args,
    const batbox::bats::BatContext& context,
    const std::function<std::future<Json>(const Json&)>& canUseGadget,
    batbox::bats::RunBatParams& params);
  std::string executePoetryShell(const std::string& text,
    const std::string& slashCommandName,const std::optional<std::string>& shell,
    const std::vector<std::string>& allowedGadgets,
    const batbox::bats::BatContext& context);
  batbox::skills::process::PoetryCommandServices poetryServices(
    const batbox::bats::BatContext& context);
  batbox::skills::RuleEntries rules(const Json& context,const std::string& behavior)const;
  Inputs inputs_;
  std::shared_ptr<batbox::skills::SkillRoster> roster_;
  batbox::skills::SkillRosterServices services_;
  std::map<std::string,batbox::skills::SkillCommand> commandIndex_;
  std::shared_ptr<application::media::AggregateProvider> attachments_;
  std::shared_ptr<application::hooks::Provider> hooks_;
  std::function<void(const batbox::bats::RunBatParams&,const query::Yield&)> runBat_;
  SkillBatLauncher launchSkillBat_;
  std::function<Json(const Json&,const std::string&,
    const batbox::bats::BatContext&)> mcpPoetry_;
};
}
