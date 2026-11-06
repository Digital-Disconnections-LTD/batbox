#include "application_config/ConfigPanelInternal.hpp"
#include "application_commands/ProvidersCommand.hpp"
#include "application_config/Discovery.hpp"
#include "providers/WireClients.hpp"
#include "transport/Errors.hpp"
#include "application_config/EndpointStore.hpp"
#include "application/Config.hpp"
#include "application_model/LocalModelService.hpp"
#include "runtime/IndigenousPath.hpp"
#include "app_state/State.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <memory>
#include <iterator>
#include <string>
#include <thread>
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
void endpointBetas(const std::filesystem::path& root){
  using namespace application_config;
  ConfigRoot environment(root);
  const auto path=root/"betas-client.json";
  application::EndpointRecord endpoint;
  endpoint.label="Betas";endpoint.baseUrl="http://127.0.0.1:8000";
  endpoint.contextWindow=16384;endpoint.maxOutputTokens=2048;
  endpoint.models={"alpha"};
  saveEndpoints(path,{{"plain",endpoint}},profileId("plain","alpha"));
  {std::ifstream file(path);
    assert(!Json::parse(file).at("endpoints").at("plain").contains("betas"));}
  assert(readEndpoints(path)->at("plain").betas.empty());
  assert(inferenceProfile(endpoint,"alpha").betas.empty());
  assert(application_model::requestBetas(
    runtimeProfile(profileId("plain","alpha"),endpoint,"alpha")).empty());
  const std::vector<std::string> betas{"context-1m-2025-08-07",
    "interleaved-thinking-2025-05-14"};
  endpoint.betas=betas;
  saveEndpoints(path,{{"beta",endpoint}},profileId("beta","alpha"));
  {std::ifstream file(path);
    assert(Json::parse(file).at("endpoints").at("beta").at("betas")==Json(betas));}
  assert(readEndpoints(path)->at("beta").betas==betas);
  assert(inferenceProfile(endpoint,"alpha").betas==betas);
  auto profile=runtimeProfile(profileId("beta","alpha"),endpoint,"alpha");
  assert(profile.betas==betas&&application_model::requestBetas(profile)==betas);
  for(const auto* kind:{"bedrock","vertex"}){
    profile.provider=kind;assert(application_model::requestBetas(profile)==betas);
  }
  profile.provider="openai-chat";
  assert(profile.betas==betas&&application_model::requestBetas(profile).empty());
  {std::ofstream file(path);file<<Json{{"profile","named"},{"profiles",{{"named",{
    {"model","alpha"},{"base_url","http://127.0.0.1:8000"},{"betas",betas}}}}},
    {"endpoints",{{"beta",{{"label","Betas"},{"base_url","http://127.0.0.1:8000"},
      {"context_window",16384},{"max_output_tokens",2048},{"models",{"alpha"}},
      {"betas",betas}}}}}}.dump();}
  const auto loaded=application::loadConfig({"--help","--config",runtime::pathToUtf8(path)});
  assert(loaded.profiles.at("named").betas==betas);
  assert(loaded.endpoints.at("beta").betas==betas);
  for(const auto* bad:{R"([""])",R"(["a,b"])",R"(["a b"])",R"(["\u00e9"])",R"([1])",
      R"("context-1m-2025-08-07")"}){
    application::Config rejected;
    bool threw=false;
    try{loadEndpoints(rejected,Json{{"endpoints",{{"bad",{{"label","Bad"},
      {"base_url","http://127.0.0.1:8000"},{"context_window",16384},
      {"max_output_tokens",2048},{"models",{"alpha"}},{"betas",Json::parse(bad)}}}}}});}
    catch(const std::invalid_argument&){threw=true;}
    assert(threw);
  }
}
const std::vector<std::string> bothBetas{"context-1m-2025-08-07",
  "interleaved-thinking-2025-05-14"};
const Json bothLimits={{"local",false},{"nonstreaming_fallback",false},
  {"request_timeout_ms",0},{"stream_idle_timeout_ms",900000},{"connect_timeout_ms",3000},
  {"nonstreaming_timeout_ms",1800000},{"max_retries",2}};
application::EndpointRecord endpointWithBoth(){
  application::EndpointRecord endpoint;
  endpoint.label="Both";endpoint.baseUrl="http://192.168.1.20:8000";
  endpoint.authHeader="x-api-key";endpoint.authToken="both-key";
  endpoint.contextWindow=16384;endpoint.maxOutputTokens=2048;
  endpoint.models={"alpha"};
  endpoint.betas=bothBetas;
  endpoint.limits=application::parseRequestLimits(bothLimits,"endpoint both");
  return endpoint;
}
void requireBoth(const application::EndpointRecord& endpoint){
  assert(endpoint.betas==bothBetas);
  assert(application::requestLimitsDocument(endpoint.limits)==bothLimits);
}
void requireBothInProfile(const application_model::ModelProfile& profile){
  assert(profile.betas==bothBetas&&application_model::requestBetas(profile)==bothBetas);
  assert(!profile.local&&!profile.nonStreamingFallback);
  assert(profile.nonStreamingTimeoutMs==1800000&&profile.maxRetries==2u);
  assert(profile.client&&profile.client->requestTimeout==std::chrono::milliseconds(0));
  assert(profile.client->streamIdleTimeout==std::chrono::milliseconds(900000));
  assert(profile.client->connectTimeout==std::chrono::milliseconds(3000));
}
void endpointBetasAndLimits(const std::filesystem::path& root){
  using namespace application_config;
  ConfigRoot environment(root);
  const auto path=root/"both-client.json";
  const auto endpoint=endpointWithBoth();
  saveEndpoints(path,{{"both",endpoint}},profileId("both","alpha"));
  {std::ifstream file(path);const auto row=Json::parse(file).at("endpoints").at("both");
    assert(row.at("betas")==Json(bothBetas));
    for(auto it=bothLimits.begin();it!=bothLimits.end();++it)
      assert(row.at(it.key())==it.value());}
  requireBoth(readEndpoints(path)->at("both"));
  const auto configured=inferenceProfile(endpoint,"alpha");
  assert(configured.betas==bothBetas&&
    application::requestLimitsDocument(configured.limits)==bothLimits);
  requireBothInProfile(runtimeProfile(profileId("both","alpha"),endpoint,"alpha"));

  Json profileRow={{"model","alpha"},{"base_url","http://192.168.1.20:8000"},
    {"betas",bothBetas}};
  profileRow.update(bothLimits);
  Json endpointRow=endpointDocument({{"both",endpoint}}).at("both");
  {std::ofstream file(path);file<<Json{{"profile","named"},
    {"profiles",{{"named",profileRow}}},{"endpoints",{{"both",endpointRow}}}}.dump();}
  const auto loaded=application::loadConfig({"--help","--config",runtime::pathToUtf8(path)});
  assert(loaded.profiles.at("named").betas==bothBetas);
  assert(application::requestLimitsDocument(loaded.profiles.at("named").limits)==bothLimits);
  requireBoth(loaded.endpoints.at("both"));
  const auto rejects=[](Json row){
    application::Config rejected;
    try{loadEndpoints(rejected,Json{{"endpoints",{{"bad",std::move(row)}}}});}
    catch(const std::invalid_argument&){return true;}
    return false;
  };
  auto badBeta=endpointRow;badBeta["betas"]=Json::array({"a,b"});
  assert(rejects(badBeta));
  auto badLimit=endpointRow;badLimit["max_retries"]=-1;
  assert(rejects(badLimit));

  // A /providers re-edit rebuilds the record from the catalog walk-through;
  // the betas and request limits it has no step for must survive the save.
  event_loop::Loop loop;
  application_commands::ProvidersInputs inputs;
  inputs.configPath=path;inputs.animate=false;
  inputs.environment=[](const std::string&)->std::optional<std::string>{return std::nullopt;};
  inputs.discover=[]{return std::vector<providers::LocalServer>{};};
  inputs.test=[](const application::EndpointRecord&){
    providers::ConnectionResult result;
    result.reached=result.ok=true;result.status=200;result.models={"alpha"};
    result.message="Connected · 1 model";
    return result;
  };
  std::optional<interactive::UiRequest> shown;
  interactive::CommandUi::Action act;
  std::optional<std::string> finished;
  application_commands::startProvidersDialog(loop,
    application_commands::completeProvidersInputs(std::move(inputs)),
    [&](interactive::CommandCompletion done){finished=done.result.value_or("");},
    [&](std::optional<interactive::UiRequest> request,interactive::CommandUi::Action action){
      shown=std::move(request);act=std::move(action);});
  const auto settle=[&](const std::function<bool()>& ready){
    for(int i=0;i<1000&&!ready();++i){
      loop.runReady();
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    assert(ready());
  };
  const auto position=[&](const std::string& choice)->std::optional<std::size_t>{
    if(!shown)return std::nullopt;
    const auto found=std::find(shown->choices.begin(),shown->choices.end(),choice);
    if(found==shown->choices.end())return std::nullopt;
    return static_cast<std::size_t>(found-shown->choices.begin());
  };
  settle([&]{return shown&&shown->id=="providers"&&position("Both").has_value()&&
    !position("Looking for local servers…").has_value();});
  act({interactive::UiAction::Kind::Activate,*position("Both"),{}});
  assert(shown&&shown->id=="providers-key"&&position("Keep the saved key")==0u);
  act({interactive::UiAction::Kind::Activate,0,{}});
  assert(shown&&shown->id=="providers-base-url");
  act({interactive::UiAction::Kind::Activate,0,{}});
  settle([&]{return shown&&shown->id=="providers-models";});
  act({interactive::UiAction::Kind::Activate,*position("alpha"),{}});
  settle([&]{return finished.has_value();});
  const auto reedited=readEndpoints(path)->at("both");
  requireBoth(reedited);
  assert(reedited.authToken=="both-key");
  requireBothInProfile(runtimeProfile(profileId("both","alpha"),reedited,"alpha"));
}
int main(){
  using namespace application_config;
  Temporary temporary;
  permissionConfig(temporary.path);
  endpointBetas(temporary.path);
  endpointBetasAndLimits(temporary.path);
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
  // Discovery goes through the endpoint's wire client; the listing it makes
  // carries the same auth headers and URL a request would.
  const auto listing=providers::endpointConfig(endpoint);
  assert(transport::modelsUrl(listing.baseUrl,listing.modelsPath)==modelsUrl(endpoint));
  assert(listing.headers.at("x-api-key")==endpoint.authToken&&listing.authToken.empty());
  assert(listing.headers.at("anthropic-version")=="2023-06-01");
  const auto found=discoverModels(endpoint,[](const application::EndpointRecord&){
    return Json::parse(R"({"data":[{"id":"alpha"},{"id":"beta"},{"id":"alpha"},{"other":"skip"}]})");});
  assert(found.success&&found.models==std::vector<std::string>({"alpha","beta"}));
  assert(discoverModels(endpoint,[](const application::EndpointRecord&)->Json{
    throw transport::APIError(503,nullptr,Json::object(),"busy");}).error==
    "Model discovery returned HTTP 503");
  assert(discoverModels(endpoint,[](const application::EndpointRecord&)->Json{
    throw transport::APIError(401,nullptr,Json::object(),"no");}).error==
    "Model discovery returned HTTP 401: the server rejected the key");
  assert(discoverModels(endpoint,[](const application::EndpointRecord&){
    return Json::parse(R"({"data":[]})");}).models.empty());
  assert(!discoverModels(endpoint,[](const application::EndpointRecord&)->Json{
    throw transport::APIConnectionError("offline");}).success);
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
  auto panelEndpoint=endpoint;
  panelEndpoint.betas=bothBetas;
  panelEndpoint.limits=application::parseRequestLimits(bothLimits,"endpoint local");
  panelConfig.endpoints={{"local",panelEndpoint}};
  saveEndpoints(path,panelConfig.endpoints,"seed",Json::object(),false);
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
  assert(visible->body.find("\nProvider  anthropic\n")!=std::string::npos);
  assert(visible->body.find("Discovery  GET http://127.0.0.1:8000/v1/models")!=std::string::npos);
  action({interactive::UiAction::Kind::Activate,3,{}});
  assert(visible&&visible->id=="endpoint-edit");
  assert(visible->body.starts_with("Provider  anthropic (read-only)\n"));
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
  requireBoth(readEndpoints(path)->at("local"));
  assert(service->config().profiles.contains(profileId("local","alpha")));
  requireBothInProfile(service->config().profiles.at(profileId("local","alpha")));

  // Max Bat depth sits with the other Bat limits; 0 is a valid depth.
  auto depthConfig=std::make_shared<application::Config>(panelConfig);
  auto depthPanel=std::make_shared<Panel>(loop,depthConfig,
    service,store,std::function<std::shared_ptr<interactive::conversation_shell::Shell>()>{},
    [](interactive::CommandCompletion){},
    [&](std::optional<interactive::UiRequest> next,
        interactive::CommandUi::Action nextAction){
      visible=std::move(next);action=std::move(nextAction);},
    [](std::string){assert(false);});
  depthPanel->start();loop.run();
  action({interactive::UiAction::Kind::Activate,2,{}});
  assert(visible&&visible->id=="endpoint-settings");
  assert(visible->choices.at(2).starts_with("Max subagents  ")&&
    visible->choices.at(3).starts_with("Concurrent subagents  ")&&
    visible->choices.at(4)=="Max Bat depth  3"&&visible->choices.at(5)=="Back");
  action({interactive::UiAction::Kind::Activate,4,{}});
  assert(visible&&visible->id=="endpoint-input"&&visible->title.find("Max Bat depth")!=std::string::npos);
  action({interactive::UiAction::Kind::TextChange,0,"-1"});
  action({interactive::UiAction::Kind::Activate,0,{}});
  assert(visible&&visible->id=="endpoint-input"&&
    visible->body.find("Enter 0 or a whole number")!=std::string::npos&&depthConfig->maxBatDepth==3);
  action({interactive::UiAction::Kind::TextChange,0,"\x7f"});
  action({interactive::UiAction::Kind::TextChange,0,"\x7f"});
  action({interactive::UiAction::Kind::TextChange,0,"0"});
  action({interactive::UiAction::Kind::Activate,0,{}});
  loop.run();
  assert(depthConfig->maxBatDepth==0);
  {std::ifstream file(path);assert(Json::parse(file).at("max_bat_depth")==0);}
  assert(visible&&visible->id=="endpoint-settings"&&
    visible->choices.at(4)=="Max Bat depth  0 (Bats cannot launch Bats)");

  // Cloud endpoints show their kind, region and project; the URL comes from the region.
  Endpoint vertex;
  vertex.label="Vertex";vertex.provider="vertex";vertex.preset="google-vertex-anthropic";
  vertex.region="us-east5";vertex.project="night-shift";
  vertex.models={"claude-sonnet-5-5@default"};vertex.contextWindow=1000;vertex.maxOutputTokens=100;
  assert(providerSummary(vertex)==
    "vertex · preset google-vertex-anthropic · region us-east5 · project night-shift");
  auto cloudConfig=panelConfig;cloudConfig.endpoints={{"vertex",vertex}};
  cloudConfig.configPath=temporary.path/"cloud-client.json";
  saveEndpoints(cloudConfig.configPath,cloudConfig.endpoints,"seed",Json::object(),false);
  auto cloudPanel=std::make_shared<Panel>(loop,std::make_shared<application::Config>(cloudConfig),
    service,store,std::function<std::shared_ptr<interactive::conversation_shell::Shell>()>{},
    [](interactive::CommandCompletion){},
    [&](std::optional<interactive::UiRequest> next,
        interactive::CommandUi::Action nextAction){
      visible=std::move(next);action=std::move(nextAction);},
    [](std::string){assert(false);});
  cloudPanel->start();loop.run();
  action({interactive::UiAction::Kind::Activate,0,{}});
  assert(visible&&visible->id=="endpoint-detail");
  assert(visible->body.starts_with("URL from region\nProvider  vertex · preset google-vertex-anthropic"));
  assert(visible->body.find("Discovery  Vertex AI in us-east5")!=std::string::npos);
  action({interactive::UiAction::Kind::Activate,3,{}});
  assert(visible&&visible->id=="endpoint-edit");
  assert(visible->body.find("region us-east5 · project night-shift (read-only)")!=std::string::npos);
  return 0;
}
