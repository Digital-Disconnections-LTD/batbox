#include "application_config/ConfigPanelInternal.hpp"
#include "application_config/Discovery.hpp"
#include "application_config/EndpointStore.hpp"
#include "application/Config.hpp"
#include "application_model/LocalModelService.hpp"
#include "runtime/IndigenousPath.hpp"
#include "app_state/State.hpp"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <memory>
#include <iterator>
#include <string>
#ifdef _WIN32
#include <cstdlib>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace batbox;
namespace {
struct Temporary {
  std::filesystem::path path;
  Temporary(){
    path=std::filesystem::temp_directory_path()/
      ("batbox-endpoint-test-"+
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path);
  }
  ~Temporary(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}
};
struct ConfigRoot {
#ifdef _WIN32
  std::optional<std::wstring> previous;
  explicit ConfigRoot(const std::filesystem::path& path){
    if(const auto* value=_wgetenv(L"BATBOX_CONFIG_DIR"))previous=value;
    _wputenv_s(L"BATBOX_CONFIG_DIR",path.c_str());
  }
  ~ConfigRoot(){_wputenv_s(L"BATBOX_CONFIG_DIR",previous?previous->c_str():L"");}
#else
  std::optional<std::string> previous;
  explicit ConfigRoot(const std::filesystem::path& path){
    if(const auto* value=std::getenv("BATBOX_CONFIG_DIR"))previous=value;
    setenv("BATBOX_CONFIG_DIR",path.c_str(),1);
  }
  ~ConfigRoot(){
    if(previous)setenv("BATBOX_CONFIG_DIR",previous->c_str(),1);
    else unsetenv("BATBOX_CONFIG_DIR");
  }
#endif
};
void permissionConfig(const std::filesystem::path& root){
  ConfigRoot environment(root);
  const auto client=root/"permission-client.json";
  const std::string saved=R"({"permissions":{"defaultMode":"plan"},"unrelated":true})";
  {std::ofstream file(client);file<<saved;}
  {std::ofstream file(root/"settings.json");
    file<<R"JSON({"permissions":{"deny":["Bash(rm:*)"],"ask":["WebFetch"],"allow":["Read"]}})JSON";}
  const auto load=[&](std::vector<std::string> flags){
    flags.insert(flags.begin(),{"--help","--config",runtime::pathToUtf8(client)});
    return application::loadConfig(flags);
  };
  const auto layered=load({});
  assert(layered.permissions.at("defaultMode")=="plan");
  assert(layered.permissions.at("deny")==Json::array({"Bash(rm:*)"}));
  assert(layered.permissions.at("ask")==Json::array({"WebFetch"}));
  assert(layered.permissions.at("allow")==Json::array({"Read"}));
  assert(load({"--print"}).permissions.at("defaultMode")=="bypassPermissions");
  assert(load({"--print"}).permissions.at("deny")==layered.permissions.at("deny"));
  for(const auto* alias:{"--yolo","--dangerously-skip-permissions"}){
    assert(load({alias}).permissions.at("defaultMode")=="bypassPermissions");
    assert(load({"--print",alias}).permissions.at("defaultMode")=="bypassPermissions");
  }
  for(const auto* mode:{"default","acceptEdits","bypassPermissions","dontAsk","plan"}){
    assert(load({"--permission-mode",mode}).permissions.at("defaultMode")==mode);
    assert(load({"--print","--permission-mode",mode}).permissions.at("defaultMode")==mode);
  }
  for(const auto& flags:std::vector<std::vector<std::string>>{{"--permission-mode"},
      {"--permission-mode","automatic"}}){
    bool rejected=false;
    try{(void)load(flags);}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
  }
  // A client default must not erase the shared setting that disables bypass.
  {std::ofstream file(root/"settings.json");
    file<<R"({"permissions":{"disableBypassPermissionsMode":"disable"}})";}
  assert(load({"--print"}).permissions.at("defaultMode")=="dontAsk");
  assert(load({"--print","--permission-mode","plan"}).permissions.at("defaultMode")=="plan");
  bool denied=false;
  try{(void)load({"--permission-mode","bypassPermissions"});}
  catch(const std::invalid_argument&){denied=true;}
  assert(denied);
  for(const auto* alias:{"--yolo","--dangerously-skip-permissions"}){
    bool rejected=false;
    try{(void)load({alias});}catch(const std::invalid_argument&){rejected=true;}
    assert(rejected);
  }
  {std::ifstream file(client);const std::string after((std::istreambuf_iterator<char>(file)),{});
    assert(after==saved);}
  std::filesystem::remove(root/"settings.json");
  {std::ofstream file(client);
    file<<R"({"permissions":{"disableBypassPermissionsMode":"disable"}})";}
  assert(load({"--print"}).permissions.at("defaultMode")=="dontAsk");
  denied=false;
  try{(void)load({"--print","--permission-mode","bypassPermissions"});}
  catch(const std::invalid_argument&){denied=true;}
  assert(denied);
  assert(application::commandLineHelp().find("--permission-mode")!=std::string::npos);
}
application_model::ModelProfile seedProfile(){
  application_model::ModelProfile profile;
  profile.id="seed";profile.wireModel="seed";
  profile.contextTokens=8192;profile.maxOutputTokens=1024;
  profile.costs={{"inputTokens",0},{"outputTokens",0},
    {"promptCacheReadTokens",0},{"promptCacheWriteTokens",0},
    {"webSearchRequests",0}};
  return profile;
}
std::shared_ptr<application_model::LocalModelService> backend(event_loop::Loop& loop){
  application_model::Config settings;
  settings.mainModel=settings.smallModel=settings.visionModel="seed";
  settings.profiles.emplace("seed",seedProfile());
  application_model::Imports imports;
  imports.authToken=[]{return std::string{};};
  imports.invalidateAuthToken=[]{};
  imports.debug=[](const std::string&,const std::string&){};
  imports.diagnostic=imports.debug;
  imports.normalizeAttachmentForAPI=[](const Json& value){return value;};
  imports.normalizeGadgetInputForAPI=[](const gadgets::Gadget&,const Json& value){return value;};
  imports.gadgetSchemaSync=[](const gadgets::Gadget&,const gadgets::GadgetSchemaOptions&){
    return Json::object();};
  imports.gadgetSchemaAsync=[&loop](const gadgets::Gadget&,
      const gadgets::GadgetSchemaOptions&){
    event_loop::Deferred<Json> result(loop);result.resolve(Json::object());return result;};
  return std::make_shared<application_model::LocalModelService>(loop,
    std::move(settings),std::move(imports));
}
}
int main(){
  using namespace application_config;
  Temporary temporary;
  permissionConfig(temporary.path);
  const auto path=temporary.path/"native-client.json";
  {std::ofstream file(path);file<<"{\"unknown\":{\"keep\":true}}";}
  application::EndpointRecord endpoint;
  endpoint.label="Local server";
  endpoint.baseUrl="http://127.0.0.1:8000/v1";
  endpoint.authHeader="x-api-key";
  endpoint.authToken=std::string(13,'k');
  endpoint.contextWindow=16384;endpoint.maxOutputTokens=2048;
  endpoint.models={"alpha","beta"};
  const auto selected=profileId("local","beta");
  saveEndpoints(path,{{"local",endpoint}},selected);
  assert(readEndpoints(path)->at("local").models.size()==2);
  {std::ifstream file(path);const auto saved=Json::parse(file);
    assert(saved.at("unknown").at("keep")==true);
    assert(saved.at("profile")==selected);}
#ifndef _WIN32
  struct stat info{};assert(::stat(path.c_str(),&info)==0);
  assert((info.st_mode&077)==0);
#endif
  assert(modelsUrl(endpoint)=="http://127.0.0.1:8000/v1/models");
  assert(inferenceProfile(endpoint,"alpha").messagesPath=="/messages");
  auto explicitPath=endpoint;explicitPath.modelsPath="/v1/models";
  assert(modelsUrl(explicitPath)=="http://127.0.0.1:8000/v1/models");
  auto fullUrl=endpoint;
  fullUrl.baseUrl="http://127.0.0.1:8000/v1/messages";
  assert(modelsUrl(fullUrl)=="http://127.0.0.1:8000/v1/models");
  assert(inferenceProfile(fullUrl,"alpha").baseUrl=="http://127.0.0.1:8000");
  auto customThinking=endpoint;customThinking.sendDisabledThinking=true;
  assert(runtimeProfile(profileId("local","alpha"),customThinking,"alpha")
    .sendDisabledThinking);
  application::Config implicit;implicit.model="seed";
  assert(application::selectedInferenceProfile(implicit).sendDisabledThinking);
  std::string requestedUrl,authorization;
  const auto fake=[&](const mcp::http::Request& request){
    requestedUrl=request.url;
    authorization=request.headers.at("x-api-key").get<std::string>();
    mcp::http::Response response;response.status=200;
    response.body=R"({"data":[{"id":"alpha"},{"id":"beta"},{"id":"alpha"},{"other":"skip"}]})";
    return response;
  };
  const auto found=discoverModels(endpoint,fake);
  assert(found.success&&found.models==std::vector<std::string>({"alpha","beta"}));
  assert(requestedUrl==modelsUrl(endpoint)&&authorization==endpoint.authToken);
  assert(discoverModels(endpoint,[](const auto&){mcp::http::Response r;
    r.status=503;return r;}).error=="Model discovery returned HTTP 503");
  assert(discoverModels(endpoint,[](const auto&){mcp::http::Response r;
    r.status=200;r.body=R"({"data":[]})";return r;}).models.empty());
  assert(!discoverModels(endpoint,[](const auto&)->mcp::http::Response{
    throw std::runtime_error("offline");}).success);
  saveEndpoints(path,{{"local",endpoint}},"seed",Json::object(),false);
  {std::ifstream file(path);assert(!Json::parse(file).contains("profile"));}
  application::Config loaded;
  {std::ifstream file(path);loadEndpoints(loaded,Json::parse(file));}
  assert(loaded.endpoints.at("local").models.size()==2);
  event_loop::Loop loop;
  auto service=backend(loop);
  auto profile=seedProfile();
  profile.id=selected;profile.wireModel="beta";
  profile.client.emplace();
  profile.client->baseUrl="http://127.0.0.1:8000/v1";
  profile.client->messagesPath="/messages";
  service->upsertRuntimeProfile(profile,endpoint.authToken);
  assert(service->config().profiles.at(selected).wireModel=="beta");
  auto fallbackService=backend(loop);
  fallbackService->upsertRuntimeProfile(profile,endpoint.authToken);
  fallbackService->removeRuntimeProfile("seed");
  assert(fallbackService->config().mainModel==selected);
  assert(fallbackService->config().smallModel==selected);
  assert(fallbackService->config().visionModel==selected);
  service->removeRuntimeProfile(selected);
  assert(!service->config().profiles.contains(selected));

  // The same UiRequest path used by the FTXUI host must never echo a typed key.
  application::Config panelConfig;
  panelConfig.configPath=path;panelConfig.model="seed";
  panelConfig.endpoints={{"local",endpoint}};
  auto state=std::make_shared<app_state::AppState>();
  state->fields=std::make_shared<value::SharedRecord>(value::SharedRecord::fromJson(
    Json{{"mainLoopModel","seed"}}));
  auto store=std::make_shared<app_state::AppStateStore>(state);
  std::optional<interactive::UiRequest> visible;
  interactive::CommandUi::Action action;
  auto panel=std::make_shared<Panel>(loop,std::make_shared<application::Config>(panelConfig),
    service,store,std::function<std::shared_ptr<interactive::conversation_shell::Shell>()>{},
    [](interactive::CommandCompletion){},
    [&](std::optional<interactive::UiRequest> next,
        interactive::CommandUi::Action nextAction){
      visible=std::move(next);action=std::move(nextAction);},
    [](std::string){assert(false);});
  panel->start();loop.run();
  assert(visible&&visible->id=="endpoint-home");
  action({interactive::UiAction::Kind::Activate,0,{}});
  assert(visible&&visible->id=="endpoint-detail");
  action({interactive::UiAction::Kind::Activate,3,{}});
  assert(visible&&visible->id=="endpoint-edit");
  action({interactive::UiAction::Kind::Activate,5,{}});
  assert(visible&&visible->id=="endpoint-input");
  const std::string typed(14,'x');
  action({interactive::UiAction::Kind::TextChange,0,typed});
  assert(visible&&visible->sensitiveInput);
  assert(visible->body.find(typed)==std::string::npos);
  assert(visible->body.find("****")!=std::string::npos);
  action({interactive::UiAction::Kind::Activate,0,{}});
  assert(visible&&visible->id=="endpoint-edit");
  assert(visible->body.find(typed)==std::string::npos);
  bool onSave=false;
  for(int step=0;step<20;++step){
    if(visible->choices.at(visible->selectedIndex)=="Save endpoint"){
      onSave=true;break;
    }
    const auto next=(visible->selectedIndex+1)%visible->choices.size();
    action({interactive::UiAction::Kind::MoveSelection,next,"down"});
  }
  assert(onSave);
  action({interactive::UiAction::Kind::Activate,visible->selectedIndex,{}});
  loop.run();
  assert(readEndpoints(path)->at("local").authToken==typed);
  assert(service->config().profiles.contains(profileId("local","alpha")));
  return 0;
}
