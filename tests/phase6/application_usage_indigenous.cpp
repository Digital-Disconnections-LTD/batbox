#include "application_usage/UsageCommand.hpp"
#include "application_commands/PermissionCommand.hpp"
#include "bats/ForkedBat.hpp"
#include "application_engine/AutoPolicy.hpp"
#include "project_recovery/Recovery.hpp"
#include "runtime/Platform.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace batbox;
namespace {
template<class T>T await(event_loop::Loop& loop,event_loop::Deferred<T> pending){
  std::optional<T> value;std::exception_ptr error;
  pending.then([&](const T* result,const std::exception_ptr& failure){
    error=failure;if(result)value=*result;
  });
  loop.run();if(error)std::rethrow_exception(error);
  if(!value)throw std::logic_error("Usage command did not settle");
  return std::move(*value);
}
void check(bool condition,const char* message){
  if(!condition)throw std::logic_error(message);
}
std::shared_ptr<application_model::LocalModelService> fixtureModel(event_loop::Loop& loop){
  application_model::Config config;
  config.mainModel=config.smallModel="fixture";
  config.client.baseUrl="http://user:secret@127.0.0.1:9001/private?token=secret";
  application_model::ModelProfile profile;
  profile.id="fixture";profile.contextTokens=100000;profile.maxOutputTokens=4000;
  profile.costs={{"inputTokens",0},{"outputTokens",0},
    {"promptCacheReadTokens",0},{"promptCacheWriteTokens",0},
    {"webSearchRequests",0}};
  config.profiles.emplace("fixture",profile);
  application_model::Imports imports;
  imports.authToken=[]{return std::string{};};
  imports.invalidateAuthToken=[]{ };
  imports.debug=[](const std::string&,const std::string&){};
  imports.diagnostic=imports.debug;
  imports.normalizeAttachmentForAPI=[](const Json& value){return value;};
  imports.normalizeGadgetInputForAPI=[](const gadgets::Gadget&,const Json& value){return value;};
  imports.gadgetSchemaSync=[](const gadgets::Gadget&,const gadgets::GadgetSchemaOptions&){return Json::object();};
  imports.gadgetSchemaAsync=[&loop](const gadgets::Gadget&,const gadgets::GadgetSchemaOptions&){
    event_loop::Deferred<Json> result(loop);result.resolve(Json::object());return result;};
  return std::make_shared<application_model::LocalModelService>(loop,config,imports);
}
}
int main(){
  event_loop::Loop loop;
  const auto root=std::filesystem::temp_directory_path()/
    ("batbox-usage-"+platform::randomUUID());
  std::filesystem::create_directories(root);
  try{
    std::ofstream(root/"BATBOX.md")<<"Use Batbox commands.\n";
    project_recovery::init(root);
    std::ofstream(root/".batbox/Makefile")<<"CURRENT := verify_usage\n";
    application::Config config;
    config.cwd=root;config.stateDirectory=root/"state";
    config.configDirectory=root/"config";config.configPath=config.configDirectory/"config.json";
    config.model="fixture";config.contextWindow=100000;
    config.maxOutputTokens=4000;config.compactAt=80000;
    std::filesystem::create_directories(config.configDirectory);
    std::ofstream(config.configPath)<<"{\"unrelated\":\"preserved\"}\n";
    auto state=std::make_shared<app_state::AppState>();
    state->fields=std::make_shared<value::SharedRecord>();
    state->fields->set("settings",Json::object());
    auto store=std::make_shared<app_state::AppStateStore>(state);
    auto policy=application_compaction_settings::Policy::create(loop,config,store);
    application_poetry::Inputs poetryInputs;poetryInputs.config=config;
    auto poetry=std::make_shared<application_poetry::Provider>(loop,std::move(poetryInputs));
    auto backend=fixtureModel(loop);
    interactive::PresentationState view;
    view.messages.push_back(std::make_shared<Json>(Json{{"type","user"},
      {"message",{{"content","Hello"}}}}));
    view.messages.push_back(std::make_shared<Json>(Json{{"type","assistant"},
      {"message",{{"id","m-1"},{"model","fixture"},
        {"content",Json::array({Json{{"type","text"},{"text","Hi"}}})},
        {"usage",{{"input_tokens",100},{"cache_read_input_tokens",20},
          {"cache_creation_input_tokens",10},{"output_tokens",7}}}}}}));
    application_usage::Sources sources;
    sources.config=config;sources.view=&view;sources.model=backend;
    sources.store=store;sources.compactionPolicy=policy;
    sources.poetry=poetry;sources.flightId=[]{return std::string("session-1");};
    sources.runtimeMemory=[] {return Json{{"liveWorkspaceGenerations",2},
      {"dormantWorkspaceRecords",1},{"retainedChildContexts",3},
      {"retainedStreamBytes",4096}};};
    sources.interactive=false;
    auto status=application_usage::makeStatusCommand(loop,{},sources);
    auto statusText=await(loop,await(loop,status.loadLocal())(""));
    const auto rendered=statusText.at("value").get<std::string>();
    check(rendered.find("Last context: 130 / 100000")!=std::string::npos,
      "Last context must use backend input, cache read and cache write");
    check(rendered.find("http://127.0.0.1:9001")!=std::string::npos,
      "Status must identify the configured endpoint");
    check(rendered.find("secret")==std::string::npos,
      "Endpoint credentials and path must not be displayed");
    check(rendered.find("$0")==std::string::npos,
      "Unknown local cost must not appear as zero dollars");
    check(rendered.find("Workspace generations: 2 live · 1 dormant")!=std::string::npos&&
      rendered.find("Stream buffers: 4096 bytes")!=std::string::npos,
      "Optional native runtime lifetime counts must be rendered as counts");

    auto context=application_usage::makeContextCommand(loop,{},sources);
    const auto contextText=await(loop,await(loop,context.loadLocal())(""));
    const auto report=contextText.at("value").get<std::string>();
    check(report.find("Context: 130 / 100000 tokens")!=std::string::npos,
      "Context headline must use the same backend usage as status");
    check(report.find("backend measured")!=std::string::npos,
      "Measured total must be labeled");
    check(report.find("Project recovery")!=std::string::npos,
      "Injected recovery packet must appear in estimated categories");
    check(report.find("Loaded instructions / memory")!=std::string::npos,
      "Injected instructions must appear in estimated categories");
    check(report.find("Estimated usage by category")!=std::string::npos,
      "Category counts must be labeled estimated");
    const auto livePolicy=[&]{
      const auto current=store->getState()->snapshot();
      return application_engine::autoCompactDependencies(config,
        application_engine::autoCompactSetting(current),"fixture",backend.get(),
        value::property(current,"settings"));
    };
    auto updated=await(loop,await(loop,context.loadLocal())("auto off"));
    check(updated.at("value").get<std::string>().find("Auto compact: off")!=
        std::string::npos&&!policy->enabled()&&
        value::property(value::property(store->getState()->snapshot(),"settings"),
          "autoCompactEnabled")==false,
      "Disabling auto compact must update the live policy and AppState");
    updated=await(loop,await(loop,context.loadLocal())("threshold 45000"));
    check(policy->compactAt()==45000&&
        value::property(value::property(store->getState()->snapshot(),"settings"),
          "compactAt")==45000,
      "Threshold edit must update the live policy and AppState");
    updated=await(loop,await(loop,context.loadLocal())("auto on"));
    check(policy->enabled()&&updated.at("value").get<std::string>().find(
        "Auto compact near: 45000 tokens")!=std::string::npos,
      "Next context analysis must use the new live threshold");
    check(getAutoCompactThreshold(livePolicy())==45000&&isAutoCompactEnabled(livePolicy()),
      "Engine policy must read changed AppState despite its captured startup config");
    {std::ifstream saved(config.configPath);const auto document=Json::parse(saved);
      check(document.at("unrelated")=="preserved"&&
        document.at("autoCompactEnabled")==true&&
        document.at("compact_at")==45000,
        "Policy write must preserve unrelated native configuration");}
    bool rejected=false;
    try{(void)await(loop,await(loop,context.loadLocal())("threshold 0"));}
    catch(const std::invalid_argument&){rejected=true;}
    check(rejected&&policy->compactAt()==45000,
      "Invalid threshold must leave the persisted live policy unchanged");

    std::optional<interactive::UiRequest> modal;
    std::string uiError;
    user_input::UiCommandRouter::Projection projection;
    projection.onUiChanged=[&](auto value){modal=std::move(value);};
    projection.onError=[&](const std::string& value){uiError=value;};
    projection.onQueueBlocked=[](bool){};
    projection.onPoetryFocus=[](bool){};
    auto router=user_input::UiCommandRouter::create(loop,std::move(projection));
    auto interactiveSources=sources;interactiveSources.interactive=true;
    auto interactiveContext=application_usage::makeContextCommand(
      loop,router,interactiveSources);
    auto open=interactiveContext.invokeUi("");
    loop.run();
    check(uiError.empty()&&modal&&modal->kind==interactive::UiRequest::Kind::Select&&
        modal->choices.size()==3,
      "Interactive context must expose compaction controls");
    router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,0,{}});
    loop.run();
    check(!policy->enabled()&&modal&&modal->choices[0].find("off")!=std::string::npos,
      "Interactive auto-compact toggle did not apply live");
    router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,1,{}});
    check(modal&&modal->kind==interactive::UiRequest::Kind::Form,
      "Threshold choice must open a form");
    router->ui()->dispatchAction({interactive::UiAction::Kind::TextChange,0,"55000"});
    router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,0,{}});
    loop.run();
    check(policy->compactAt()==55000&&modal&&
        modal->choices[1].find("55000")!=std::string::npos,
      "Interactive threshold form did not apply live");
    router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,2,{}});
    (void)await(loop,open);
    check(uiError.empty()&&!modal,"Interactive context did not close cleanly");

    const Json rules={{"mode","default"},{"isBypassPermissionsModeAvailable",false},
      {"alwaysDenyRules",Json::array({"Bash(rm:*)"})},
      {"alwaysAskRules",Json::array({"WebFetch"})}};
    store->setState([&](const app_state::AppStateRef& prior){
      auto next=std::make_shared<app_state::AppState>(prior->shallowCopy());
      next->setPermissionContext(std::make_shared<value::SharedRecord>(
        value::SharedRecord::fromJson(rules)));return next;});
    const auto currentPermission=[&]{return store->getState()->permissionContext->snapshot();};
    auto transitions=std::make_shared<bootstrap::FlightTransitions>();
    auto permissions=application_commands::makePermissionsCommand(loop,router,
      config,store,transitions);
    check(permissions.descriptor.at("aliases")==Json::array({"permission","allowed-tools"}),
      "Permission command aliases must be discoverable");
    auto choosing=permissions.invokeUi("");loop.run();
    check(modal&&modal->kind==interactive::UiRequest::Kind::Select&&
        modal->selectedIndex==0&&modal->choices.size()==5&&
        modal->choices[0].find("current")!=std::string::npos,
      "Permission selector must show and select the current mode");
    router->ui()->dispatchAction({interactive::UiAction::Kind::MoveSelection,2,"down"});
    check(modal&&modal->selectedIndex==2&&currentPermission()==rules,
      "Moving through permission choices must not apply them");
    router->ui()->dispatchAction({interactive::UiAction::Kind::Cancel,0,{}});
    (void)await(loop,choosing);
    check(currentPermission()==rules&&!modal,"Escape must preserve permission state");
    choosing=permissions.invokeUi("");loop.run();
    router->ui()->dispatchAction({interactive::UiAction::Kind::MoveSelection,2,"down"});
    router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,2,{}});
    const auto applied=await(loop,choosing);
    check(currentPermission().at("mode")=="bypassPermissions"&&
        currentPermission().at("isBypassPermissionsModeAvailable")==true&&
        applied.result&&applied.result->find("bypassPermissions")!=std::string::npos,
      "Enter must update the live permission context and report the selected mode");
    check(currentPermission().at("alwaysDenyRules")==rules.at("alwaysDenyRules")&&
        currentPermission().at("alwaysAskRules")==rules.at("alwaysAskRules"),
      "Mode selection must preserve deny and mandatory-ask rules");
    (void)await(loop,permissions.invokeUi("plan"));
    check(currentPermission().at("mode")=="plan"&&!transitions->needsPlanModeExitAttachment,
      "Entering plan mode must use the flight transition state");
    (void)await(loop,permissions.invokeUi("dontAsk"));
    check(currentPermission().at("mode")=="dontAsk"&&transitions->hasExitedPlanMode&&
        transitions->needsPlanModeExitAttachment,
      "Leaving plan mode must schedule its exit attachment");
    choosing=permissions.invokeUi("");loop.run();
    // Policy may change while the selector is open; check it again on Enter.
    store->setState([](const app_state::AppStateRef& prior){
      auto next=std::make_shared<app_state::AppState>(prior->shallowCopy());
      next->fields->set("settings",Json{{"permissions",
        {{"disableBypassPermissionsMode","disable"}}}});return next;});
    router->ui()->dispatchAction({interactive::UiAction::Kind::Activate,2,{}});
    check(currentPermission().at("mode")=="dontAsk"&&modal&&
        modal->choices[2].find("disabled by policy")!=std::string::npos&&
        modal->body.find("disabled by settings")!=std::string::npos,
      "Disabled bypass must remain unavailable when policy changes in an open selector");
    router->ui()->dispatchAction({interactive::UiAction::Kind::Cancel,0,{}});
    (void)await(loop,choosing);
    choosing=permissions.invokeUi("unsupported");loop.run();
    check(modal&&modal->body.find("Unknown permission mode")!=std::string::npos&&
        currentPermission().at("mode")=="dontAsk",
      "Unknown modes must not modify permissions");
    router->ui()->dispatchAction({interactive::UiAction::Kind::Cancel,0,{}});
    (void)await(loop,choosing);
    auto printConfig=config;printConfig.print=true;
    auto headlessPermissions=application_commands::makePermissionsCommand(loop,router,
      printConfig,store,transitions);
    const auto headlessStatus=await(loop,headlessPermissions.invokeUi(""));
    check(!modal&&headlessStatus.result&&
        headlessStatus.result->find("dontAsk")!=std::string::npos,
      "Headless permission status must complete without opening a selector");
    const auto headlessDenied=await(loop,headlessPermissions.invokeUi("bypassPermissions"));
    check(!modal&&headlessDenied.result&&
        headlessDenied.result->find("disabled")!=std::string::npos&&
        currentPermission().at("mode")=="dontAsk",
      "A denied headless mode change must return immediately without UI");
    {std::ifstream saved(config.configPath);const auto document=Json::parse(saved);
      check(!document.contains("permissions")&&document.at("unrelated")=="preserved",
        "Current-flight permission choices must not rewrite saved configuration");}
    std::filesystem::remove_all(root);
    std::cout<<"native context/status usage checks passed\n";
    return 0;
  }catch(...){std::filesystem::remove_all(root);throw;}
}
