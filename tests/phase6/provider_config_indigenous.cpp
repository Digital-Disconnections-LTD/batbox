#include "application_config/ConfigPanelInternal.hpp"
#include "application_config/EndpointStore.hpp"
#include "application/Config.hpp"
#include "runtime/IndigenousPath.hpp"
#include "transport/WireClient.hpp"
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <cstdlib>
#endif

using namespace batbox;
using namespace batbox::application_config;
namespace {
struct Temporary {
  std::filesystem::path path;
  Temporary(){
    path=std::filesystem::temp_directory_path()/
      ("batbox-provider-config-"+
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
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
bool contains(const std::string& text,const std::string& part){
  return text.find(part)!=std::string::npos;
}
Json endpointsDocument(){
  return Json{{"profile","chat::qwen"},{"endpoints",{
    {"messages",{{"label","Messages"},{"base_url","http://127.0.0.1:8000"},
      {"context_window",32000},{"max_output_tokens",4000},{"models",{"qwen"}}}},
    {"chat",{{"label","Chat"},{"base_url","http://127.0.0.1:8000/v1"},
      {"provider","openai-chat"},{"preset","vllm"},
      {"quirks",{{"max_tokens_field","max_completion_tokens"},{"tool_ids","short"},
        {"usage_in_stream",false}}},
      {"models_path","/v1/models"},
      {"context_window",32000},{"max_output_tokens",4000},{"models",{"qwen"}}}}}}};
}
void kindsAndQuirks(){
  assert(transport::providerKinds()==std::vector<std::string>(
    {"anthropic","openai-chat","bedrock","vertex"}));
  assert(transport::isProviderKind("openai-chat")&&!transport::isProviderKind("openai"));
  const auto unknown=failure([]{transport::validateProviderKind("openai");});
  assert(contains(unknown,"\"openai\"")&&contains(unknown,"anthropic, openai-chat, bedrock, vertex"));
  assert(transport::defaultMessagesPath("openai-chat")=="/v1/chat/completions");
  assert(transport::defaultMessagesPath("anthropic")=="/v1/messages");
  const auto defaults=transport::chatQuirks(Json::object());
  assert(defaults.maxTokensField=="max_tokens"&&defaults.reasoning=="none");
  assert(defaults.toolIds=="any"&&defaults.usageInStream&&defaults.systemRole=="system");
  const auto custom=transport::chatQuirks({{"max_tokens_field","max_completion_tokens"},
    {"reasoning","openrouter"},{"tool_ids","short"},{"usage_in_stream",false},
    {"system_role","developer"}});
  assert(custom.maxTokensField=="max_completion_tokens"&&custom.reasoning=="openrouter");
  assert(custom.toolIds=="short"&&!custom.usageInStream&&custom.systemRole=="developer");
  assert(contains(failure([]{transport::validateQuirks({{"tool_id","short"}});}),"Unknown quirk \"tool_id\""));
  assert(contains(failure([]{transport::validateQuirks({{"reasoning","deepseek"}});}),
    "none, openai, reasoning_content, openrouter"));
  assert(contains(failure([]{transport::validateQuirks({{"usage_in_stream","yes"}});}),"true or false"));
  assert(contains(failure([]{transport::validateQuirks(Json::array());}),"JSON object"));
  assert(transport::modelsUrl("http://h:1","")=="http://h:1/v1/models");
  assert(transport::modelsUrl("http://h:1/v1/","")=="http://h:1/v1/models");
  assert(transport::modelsUrl("http://h:1/v1","/v1/models")=="http://h:1/v1/models");
  assert(transport::modelsUrl("http://h:1/api","/tags")=="http://h:1/api/tags");
  transport::ClientConfig client;
  assert(transport::AnthropicMessagesClient(client).kind()=="anthropic");
}
void endpointRoundTrip(const std::filesystem::path& root){
  const auto path=root/"native-client.json";
  auto document=endpointsDocument();
  document["unrelated"]=true;
  write(path,document);
  const auto loaded=readEndpoints(path);
  assert(loaded&&loaded->size()==2);
  const auto& messages=loaded->at("messages");
  assert(messages.provider=="anthropic"&&messages.preset.empty()&&messages.quirks.empty());
  assert(messages.messagesPath=="/v1/messages");
  const auto& chat=loaded->at("chat");
  assert(chat.provider=="openai-chat"&&chat.preset=="vllm");
  assert(chat.messagesPath=="/v1/chat/completions");
  assert(chat.quirks.at("tool_ids")=="short"&&chat.quirks.at("usage_in_stream")==false);
  saveEndpoints(path,*loaded,"chat::qwen");
  const auto saved=read(path);
  assert(saved.at("unrelated")==true);
  const auto& rows=saved.at("endpoints");
  assert(!rows.at("messages").contains("provider"));
  assert(!rows.at("messages").contains("preset")&&!rows.at("messages").contains("quirks"));
  assert(rows.at("chat").at("provider")=="openai-chat");
  assert(rows.at("chat").at("preset")=="vllm");
  assert(rows.at("chat").at("messages_path")=="/v1/chat/completions");
  assert(rows.at("chat").at("quirks")==document.at("endpoints").at("chat").at("quirks"));
  const auto again=readEndpoints(path);
  assert(again->at("chat").provider==chat.provider&&again->at("chat").quirks==chat.quirks);
  assert(endpointDocument(*again)==rows);

  const auto profile=inferenceProfile(chat,"qwen");
  assert(profile.baseUrl=="http://127.0.0.1:8000/v1");
  assert(profile.messagesPath=="/chat/completions");
  assert(profile.provider=="openai-chat"&&profile.preset=="vllm"&&profile.quirks==chat.quirks);
  assert(profile.modelsPath=="/v1/models");
  assert(modelsUrl(chat)=="http://127.0.0.1:8000/v1/models");
  const auto runtime=runtimeProfile(profileId("chat","qwen"),chat,"qwen");
  assert(runtime.provider=="openai-chat"&&runtime.preset=="vllm"&&runtime.quirks==chat.quirks);
  assert(runtime.client&&runtime.client->messagesPath=="/chat/completions");
  assert(runtime.client->modelsPath=="/v1/models");
  assert(runtimeProfile("messages::qwen",messages,"qwen").provider=="anthropic");

  auto invalid=chat;invalid.provider="gemini";
  assert(contains(failure([&]{validateEndpoint("chat",invalid);}),"anthropic, openai-chat, bedrock, vertex"));
  invalid=chat;invalid.quirks={{"system_role","user"}};
  assert(contains(failure([&]{validateEndpoint("chat",invalid);}),"system, developer"));
  invalid=chat;invalid.preset="two\nlines";
  assert(!failure([&]{validateEndpoint("chat",invalid);}).empty());
  auto badDocument=endpointsDocument();
  badDocument["endpoints"]["chat"]["provider"]="openai";
  write(path,badDocument);
  assert(contains(failure([&]{(void)readEndpoints(path);}),"use one of: anthropic, openai-chat"));
}
void loadedProfiles(const std::filesystem::path& root){
  const auto path=root/"loaded-client.json";
  write(path,endpointsDocument());
  const auto config=application::loadConfig({"--config",runtime::pathToUtf8(path)});
  assert(config.model=="chat::qwen");
  const auto selected=application::selectedInferenceProfile(config);
  assert(selected.provider=="openai-chat"&&selected.preset=="vllm");
  assert(selected.messagesPath=="/chat/completions");
  assert(selected.quirks.at("max_tokens_field")=="max_completion_tokens");
  assert(config.profiles.at("messages::qwen").provider=="anthropic");

  const auto explicitPath=root/"explicit-client.json";
  write(explicitPath,Json{{"profiles",{
    {"local",{{"model","qwen"},{"base_url","http://127.0.0.1:9931"},
      {"provider","openai-chat"},{"preset","llama.cpp"},{"models_path","/v1/models"},
      {"quirks",{{"reasoning","reasoning_content"}}}}},
    {"claude",{{"model","claude"},{"base_url","http://127.0.0.1:8000"}}}}}});
  const auto explicitConfig=application::loadConfig(
    {"--config",runtime::pathToUtf8(explicitPath),"--profile","local"});
  const auto& local=explicitConfig.profiles.at("local");
  assert(local.provider=="openai-chat"&&local.preset=="llama.cpp");
  assert(local.messagesPath=="/v1/chat/completions"&&local.modelsPath=="/v1/models");
  assert(local.quirks.at("reasoning")=="reasoning_content");
  const auto& claude=explicitConfig.profiles.at("claude");
  assert(claude.provider=="anthropic"&&claude.messagesPath=="/v1/messages");

  write(explicitPath,Json{{"profiles",{{"local",{{"model","qwen"},
    {"base_url","http://127.0.0.1:9931"},{"provider","ollama"}}}}}});
  const auto unknown=failure([&]{(void)application::loadConfig(
    {"--config",runtime::pathToUtf8(explicitPath)});});
  assert(contains(unknown,"Profile local")&&contains(unknown,"openai-chat"));
  write(explicitPath,Json{{"profiles",{{"local",{{"model","qwen"},
    {"base_url","http://127.0.0.1:9931"},{"provider","openai-chat"},
    {"quirks",{{"tool_ids","long"}}}}}}}});
  assert(contains(failure([&]{(void)application::loadConfig(
    {"--config",runtime::pathToUtf8(explicitPath)});}),"any, short"));

  const auto legacyPath=root/"legacy-client.json";
  write(legacyPath,Json{{"model","qwen"},{"base_url","http://127.0.0.1:8000"}});
  const auto legacy=application::loadConfig({"--config",runtime::pathToUtf8(legacyPath)});
  assert(application::selectedInferenceProfile(legacy).provider=="anthropic");
}
void isolate(const std::filesystem::path& root){
#ifdef _WIN32
  _putenv_s("BATBOX_CONFIG_DIR",runtime::pathToUtf8(root).c_str());
  for(const auto* name:{"BATBOX_PROFILE","BATBOX_MODEL","BATBOX_BASE_URL",
      "BATBOX_AUTH_TOKEN","BATBOX_API_KEY_HELPER"})_putenv_s(name,"");
#else
  setenv("BATBOX_CONFIG_DIR",root.c_str(),1);
  for(const auto* name:{"BATBOX_PROFILE","BATBOX_MODEL","BATBOX_BASE_URL",
      "BATBOX_AUTH_TOKEN","BATBOX_API_KEY_HELPER"})unsetenv(name);
#endif
}
} // namespace

int main(){
  Temporary root;
  isolate(root.path);
  kindsAndQuirks();
  endpointRoundTrip(root.path);
  loadedProfiles(root.path);
  return 0;
}
