#include "application_config/ConfigPanelInternal.hpp"
#include "application_config/Discovery.hpp"
#include "application_config/EndpointStore.hpp"
#include "application/Config.hpp"
#include "providers/cloud/CloudClients.hpp"
#include "runtime/IndigenousPath.hpp"
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>

using namespace batbox;
using namespace batbox::application_config;
namespace {
struct Temporary {
  std::filesystem::path path;
  Temporary(){
    path=std::filesystem::temp_directory_path()/
      ("batbox-cloud-config-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(path);
  }
  ~Temporary(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}
};
void write(const std::filesystem::path& path,const Json& document){
  std::ofstream file(path);file<<document.dump(2);
}
Json read(const std::filesystem::path& path){
  std::ifstream file(path);return Json::parse(file);
}
std::string failure(const std::function<void()>& action){
  try{action();}catch(const std::invalid_argument& error){return error.what();}
  return {};
}
bool contains(const std::string& text,const std::string& part){return text.find(part)!=std::string::npos;}
Json document(){
  return Json{{"profile","bedrock::us.anthropic.claude-opus-5-5-v1:0"},{"endpoints",{
    {"bedrock",{{"label","Bedrock"},{"provider","bedrock"},{"region","us-west-2"},
      {"context_window",200000},{"max_output_tokens",8192},{"models",{"us.anthropic.claude-opus-5-5-v1:0"}}}},
    {"vertex",{{"label","Vertex"},{"provider","vertex"},{"project","my-project"},{"region","global"},
      {"context_window",200000},{"max_output_tokens",8192},{"models",{"claude-opus-5-5","claude-sonnet-5-5"}}}}}}};
}
void roundTrip(const std::filesystem::path& root){
  const auto path=root/"native-client.json";
  write(path,document());
  const auto loaded=readEndpoints(path);
  assert(loaded&&loaded->size()==2);
  const auto& bedrock=loaded->at("bedrock");
  assert(bedrock.provider=="bedrock"&&bedrock.region=="us-west-2"&&bedrock.baseUrl.empty());
  const auto& vertex=loaded->at("vertex");
  assert(vertex.provider=="vertex"&&vertex.project=="my-project"&&vertex.region=="global");
  saveEndpoints(path,*loaded,"vertex::claude-opus-5-5");
  const auto rows=read(path).at("endpoints");
  assert(rows.at("bedrock").at("region")=="us-west-2"&&!rows.at("bedrock").contains("project"));
  assert(rows.at("vertex").at("project")=="my-project"&&rows.at("vertex").at("provider")=="vertex");
  assert(endpointDocument(*readEndpoints(path))==rows);

  const auto profile=inferenceProfile(vertex,"claude-opus-5-5");
  assert(profile.provider=="vertex"&&profile.project=="my-project"&&profile.region=="global");
  const auto runtime=runtimeProfile(profileId("bedrock","m"),bedrock,"m");
  assert(runtime.provider=="bedrock"&&runtime.region=="us-west-2"&&runtime.client&&runtime.client->baseUrl.empty());

  auto invalid=bedrock;invalid.region="US West";
  assert(contains(failure([&]{validateEndpoint("bedrock",invalid);}),"region"));
  invalid=bedrock;invalid.provider="anthropic";
  assert(contains(failure([&]{validateEndpoint("bedrock",invalid);}),"HTTP(S) base URL"));
  invalid=bedrock;invalid.baseUrl="ftp://vpce";
  assert(contains(failure([&]{validateEndpoint("bedrock",invalid);}),"HTTP(S) base URL"));
  auto vpce=bedrock;vpce.baseUrl="https://vpce-1.bedrock-runtime.us-west-2.vpce.amazonaws.com";
  validateEndpoint("bedrock",vpce);
}
void startup(const std::filesystem::path& root){
  const auto path=root/"startup-client.json";
  write(path,document());
  const auto config=application::loadConfig({"--config",runtime::pathToUtf8(path)});
  assert(config.model=="bedrock::us.anthropic.claude-opus-5-5-v1:0");
  const auto selected=application::selectedInferenceProfile(config);
  assert(selected.provider=="bedrock"&&selected.region=="us-west-2"&&selected.baseUrl.empty());
  assert(!config.baseUrl.empty());

  const auto explicitPath=root/"explicit-client.json";
  write(explicitPath,Json{{"profiles",{{"vertex",{{"model","claude-opus-5-5"},{"provider","vertex"},
    {"project","p-1"},{"region","us-east5"}}}}}});
  const auto explicitConfig=application::loadConfig({"--config",runtime::pathToUtf8(explicitPath)});
  const auto& vertex=explicitConfig.profiles.at("vertex");
  assert(vertex.provider=="vertex"&&vertex.project=="p-1"&&vertex.region=="us-east5"&&vertex.baseUrl.empty());
  write(explicitPath,Json{{"profiles",{{"plain",{{"model","m"}}}}}});
  assert(contains(failure([&]{(void)application::loadConfig({"--config",runtime::pathToUtf8(explicitPath)});}),"base_url"));
}
void discoveryAndClients(){
  application::EndpointRecord vertex;
  vertex.label="Vertex";vertex.provider="vertex";vertex.project="p";vertex.region="global";
  vertex.models={"claude-opus-5-5","claude-sonnet-5-5"};
  vertex.contextWindow=1000;vertex.maxOutputTokens=100;
  // Discovery goes through the Vertex client, which lists the endpoint's models without a request.
  const auto listed=discoverModels(vertex);
  assert(listed.success&&listed.models==vertex.models);
  vertex.models.clear();
  assert(discoverModels(vertex).models.front()=="claude-opus-5-5");

  transport::ClientConfig network;
  network.baseUrl="";
  auto bedrock=providers::cloud::makeCloudClient({"bedrock","us-east-1","",{},""},network);
  auto vertexClient=providers::cloud::makeCloudClient({"vertex","global","p",{"claude-x"},"token"},network);
  assert(bedrock->kind()=="bedrock"&&vertexClient->kind()=="vertex");
  assert(vertexClient->listModels()==Json::parse(R"({"data":[{"id":"claude-x"}]})"));
  assert(contains(failure([&]{(void)providers::cloud::makeCloudClient({"anthropic","","",{},""},network);}),"Not a cloud provider"));
}
void isolate(const std::filesystem::path& root){
  setenv("BATBOX_CONFIG_DIR",root.c_str(),1);
  for(const auto* name:{"BATBOX_PROFILE","BATBOX_MODEL","BATBOX_BASE_URL","BATBOX_AUTH_TOKEN","BATBOX_API_KEY_HELPER"})
    unsetenv(name);
}
} // namespace

int main(){
  Temporary root;
  isolate(root.path);
  roundTrip(root.path);
  startup(root.path);
  discoveryAndClients();
  return 0;
}
