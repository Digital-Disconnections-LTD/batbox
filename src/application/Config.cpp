#include "application/Config.hpp"
#include "application_config/EndpointStore.hpp"
#include "app_state/PermissionModeBridge.hpp"
#include "app_state/provider/PermissionHelpers.hpp"
#include "runtime/IndigenousPath.hpp"
#ifdef _WIN32
#include "runtime/IndigenousUnicode.hpp"
#endif
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace batbox::application {
namespace {
std::string environment(const char* name){
#ifdef _WIN32
  return runtime::environmentUtf8(name);
#else
  const auto* value=std::getenv(name);return value?value:"";
#endif
}
std::filesystem::path homeDirectory(){
#ifdef _WIN32
  auto path=environment("USERPROFILE");if(path.empty())path=environment("HOME");
#else
  auto path=environment("HOME");if(path.empty())path=environment("USERPROFILE");
#endif
  if(path.empty())throw std::runtime_error("Home directory is unavailable");
  return runtime::pathFromUtf8(path);
}
Json readObject(const std::filesystem::path& path,bool required){
  std::ifstream stream(path);
  if(!stream){if(required)throw std::runtime_error("Cannot read configuration: "+runtime::pathToUtf8(path));return Json::object();}
  auto value=Json::parse(stream);
  if(!value.is_object())throw std::runtime_error("Configuration must be a JSON object: "+runtime::pathToUtf8(path));
  return value;
}
std::size_t positive(const std::string& value,const char* name){
  std::size_t used=0;unsigned long long parsed=0;
  try{parsed=std::stoull(value,&used);}catch(...){throw std::invalid_argument(std::string("Invalid ")+name);}
  if(value.empty()||value.front()=='-'||used!=value.size()||parsed==0||parsed>std::numeric_limits<std::size_t>::max())
    throw std::invalid_argument(std::string("Invalid ")+name);
  return static_cast<std::size_t>(parsed);
}
void applyConfig(Config& config,const Json& value){
  const auto text=[&](const char* name,std::string& output){if(value.contains(name))output=value.at(name).get<std::string>();};
  text("base_url",config.baseUrl);text("model",config.model);text("auth_token",config.authToken);
  text("profile",config.selectedProfile);
  text("api_key_helper",config.apiKeyHelper);text("searxng_url",config.searxngUrl);text("scrapling_command",config.scraplingCommand);
  const auto number=[&](const char* name,std::size_t& output){
    if(value.contains(name))output=positive(value::stringify(value.at(name)),name);
  };
  number("context_window",config.contextWindow);number("max_output_tokens",config.maxOutputTokens);
  number("compact_at",config.compactAt);number("max_subagents",config.maxSubagents);
  number("max_concurrent_subagents",config.maxConcurrentSubagents);
  if(value.contains("autoCompactEnabled"))
    config.settings["autoCompactEnabled"]=value.at("autoCompactEnabled").get<bool>();
  if(value.contains("fullscreen"))config.fullscreen=value.at("fullscreen").get<bool>();
  if(value.contains("thinking"))config.thinking=value.at("thinking").get<bool>();
  if(value.contains("auto_title"))config.autoTitle=value.at("auto_title").get<bool>();
  if(value.contains("fileCheckpointingEnabled"))
    config.fileCheckpointingEnabled=value.at("fileCheckpointingEnabled").get<bool>();
  if(value.contains("builtinExplorePlanAgents"))
    config.builtinExplorePlanBats=value.at("builtinExplorePlanAgents").get<bool>();
  if(value.contains("state_directory"))config.stateDirectory=
    runtime::pathFromUtf8(value.at("state_directory").get<std::string>());
  if(value.contains("mcpServers")){
    config.mcpServers=value.at("mcpServers");config.mcpServersConfigured=true;
  }
  if(value.contains("permissions")){
    const auto& permissions=value.at("permissions");
    if(!permissions.is_object())
      throw std::invalid_argument("Permissions must be a JSON object");
    config.permissions.update(permissions);
  }
  if(value.contains("hooks")){
    if(!value.at("hooks").is_object())
      throw std::invalid_argument("hooks must be a JSON object");
    config.hooks=value.at("hooks");
  }
  if(value.contains("worktree")){
    if(!value.at("worktree").is_object())
      throw std::invalid_argument("worktree must be a JSON object");
    config.worktree=value.at("worktree");
  }
  if(value.contains("agents")){
    if(!value.at("agents").is_object())
      throw std::invalid_argument("agents must be a JSON object");
    config.customBats=value.at("agents");
  }
  if(value.contains("profiles")){
    const auto& profiles=value.at("profiles");
    if(!profiles.is_object())throw std::invalid_argument("profiles must be a JSON object");
    for(auto it=profiles.begin();it!=profiles.end();++it){
      if(it.key().empty()||!it.value().is_object())
        throw std::invalid_argument("Each profile needs a name and JSON object");
      const auto& row=it.value();
      InferenceProfile profile;
      profile.contextWindow=config.contextWindow;
      profile.maxOutputTokens=config.maxOutputTokens;
      const auto field=[&](const char* name,std::string& output){
        if(row.contains(name))output=row.at(name).get<std::string>();
      };
      field("model",profile.model);field("base_url",profile.baseUrl);
      field("messages_path",profile.messagesPath);
      field("auth_token",profile.authToken);
      field("api_key_helper",profile.apiKeyHelper);
      field("api_key_env",profile.apiKeyEnv);
      field("auth_header",profile.authHeader);
      if(row.contains("headers"))profile.headers=row.at("headers");
      if(!profile.headers.is_object())
        throw std::invalid_argument("Profile headers must be a JSON object: "+it.key());
      for(auto header=profile.headers.begin();header!=profile.headers.end();++header)
        if(!header.value().is_string())
          throw std::invalid_argument("Profile header values must be strings: "+it.key());
      if(row.contains("context_window"))profile.contextWindow=
        positive(value::stringify(row.at("context_window")),"context_window");
      if(row.contains("max_output_tokens"))profile.maxOutputTokens=
        positive(value::stringify(row.at("max_output_tokens")),"max_output_tokens");
      const auto flag=[&](const char* name,bool& output){
        if(row.contains(name))output=row.at(name).get<bool>();
      };
      flag("thinking",profile.thinking);
      flag("prompt_caching",profile.poetryStashing);
      flag("adaptive_thinking",profile.adaptiveThinking);
      flag("structured_outputs",profile.structuredOutputs);
      flag("effort",profile.effort);
      flag("send_disabled_thinking",profile.sendDisabledThinking);
      config.profiles.emplace(it.key(),std::move(profile));
    }
  }
  application_config::loadEndpoints(config,value);
}
}
bool bypassPermissionsDisabled(const Config& config,const Json& effectiveSettings){
  app_state::provider::BypassPolicyServices policy;
  policy.stashedDisableGate=[&]{
    return value::property(config.permissions,"disableBypassPermissionsMode")=="disable"||
      value::property(value::property(config.settings,"permissions"),
        "disableBypassPermissionsMode")=="disable";};
  policy.getSettings=[&]{return effectiveSettings;};
  return app_state::provider::isBypassPermissionsModeDisabled(policy);
}
Config loadConfig(const std::vector<std::string>& arguments){
  Config config;const auto home=homeDirectory();config.cwd=std::filesystem::current_path();
  const auto root=environment("XDG_CONFIG_HOME");
  const auto legacyConfig=(root.empty()?home/".config":runtime::pathFromUtf8(root))/
    "batbox"/"native-client.json";
  const auto directory=environment("BATBOX_CONFIG_DIR");
  config.configDirectory=directory.empty()?home/".batbox":
    std::filesystem::absolute(runtime::pathFromUtf8(directory)).lexically_normal();
  if(directory.empty())config.legacyConfigDirectory=legacyConfig.parent_path();
  config.configPath=config.configDirectory/"native-client.json";
  const auto legacyState=home/".local"/"state"/"batbox-native";
  config.stateDirectory=directory.empty()&&std::filesystem::is_directory(legacyState)?
    legacyState:config.configDirectory/"state";
  bool explicitConfig=false;
  for(std::size_t i=0;i<arguments.size();++i)if(arguments[i]=="--config"){
    if(i+1==arguments.size())throw std::invalid_argument("--config requires a path");
    config.configPath=runtime::pathFromUtf8(arguments[++i]);explicitConfig=true;
  }
  config.settings=readObject(config.configDirectory/"settings.json",false);
  applyConfig(config,config.settings);
  const auto selectedConfig=!explicitConfig&&directory.empty()&&
    !std::filesystem::exists(config.configPath)?legacyConfig:config.configPath;
  applyConfig(config,readObject(selectedConfig,explicitConfig));
  const auto envText=[&](const char* name,std::string& value){auto selected=environment(name);if(!selected.empty())value=std::move(selected);};
  std::optional<std::string> requestedModel,requestedBaseUrl,requestedHelper,
    requestedToken;
  std::optional<std::size_t> requestedContext,requestedOutput;
  std::optional<std::string> requestedPermissionMode;
  if(auto value=environment("BATBOX_MODEL");!value.empty())requestedModel=value;
  if(auto value=environment("BATBOX_BASE_URL");!value.empty())requestedBaseUrl=value;
  if(auto value=environment("BATBOX_API_KEY_HELPER");!value.empty())requestedHelper=value;
  if(auto value=environment("BATBOX_AUTH_TOKEN");!value.empty())requestedToken=value;
  if(auto value=environment("BATBOX_CONTEXT_WINDOW");!value.empty())
    requestedContext=positive(value,"BATBOX_CONTEXT_WINDOW");
  if(auto value=environment("BATBOX_MAX_OUTPUT_TOKENS");!value.empty())
    requestedOutput=positive(value,"BATBOX_MAX_OUTPUT_TOKENS");
  envText("BATBOX_PROFILE",config.selectedProfile);
  envText("BATBOX_BASE_URL",config.baseUrl);envText("BATBOX_MODEL",config.model);
  envText("BATBOX_AUTH_TOKEN",config.authToken);envText("BATBOX_API_KEY_HELPER",config.apiKeyHelper);
  envText("BATBOX_SEARXNG_URL",config.searxngUrl);envText("BATBOX_SCRAPLING_COMMAND",config.scraplingCommand);
  const auto envNumber=[&](const char* name,std::size_t& value){const auto selected=environment(name);if(!selected.empty())value=positive(selected,name);};
  envNumber("BATBOX_MAX_OUTPUT_TOKENS",config.maxOutputTokens);envNumber("BATBOX_CONTEXT_WINDOW",config.contextWindow);
  envNumber("BATBOX_COMPACT_AT",config.compactAt);envNumber("BATBOX_MAX_SUBAGENTS",config.maxSubagents);
  envNumber("BATBOX_MAX_CONCURRENT_SUBAGENTS",config.maxConcurrentSubagents);
  std::vector<std::string> poetry;
  config.modelRequested=false;
  for(std::size_t i=0;i<arguments.size();++i){
    const auto& argument=arguments[i];
    const auto next=[&](const char* flag){
      if(i+1==arguments.size())throw std::invalid_argument(std::string(flag)+" requires a value");
      return arguments[++i];
    };
    if(argument=="--help"||argument=="-h")config.help=true;
    else if(argument=="--config"){++i;}
    else if(argument=="--profile"){
      config.selectedProfile=next("--profile");config.modelRequested=true;}
    else if(argument=="--model"){
      config.model=next("--model");requestedModel=config.model;}
    else if(argument=="--base-url"){
      config.baseUrl=next("--base-url");requestedBaseUrl=config.baseUrl;}
    else if(argument=="--api-key-helper"){
      config.apiKeyHelper=next("--api-key-helper");requestedHelper=config.apiKeyHelper;}
    else if(argument=="--cwd")config.cwd=runtime::pathFromUtf8(next("--cwd"));
    else if(argument=="--yolo"||argument=="--dangerously-skip-permissions")
      requestedPermissionMode="bypassPermissions";
    else if(argument=="--permission-mode"){
      requestedPermissionMode=next("--permission-mode");
      if(app_state::permissionModeFromString(*requestedPermissionMode,false)!=
          *requestedPermissionMode)
        throw std::invalid_argument("Unknown permission mode: "+*requestedPermissionMode);
    }
    else if(argument=="--fullscreen")config.fullscreen=true;
    else if(argument=="--inline")config.fullscreen=false;
    else if(argument=="--print"||argument=="-p")config.print=true;
    else if(argument=="--verbose")config.verbose=true;
    else if(argument=="--replay-user-messages")
      config.replayUserMessages=true;
    else if(argument=="--input-format"||
        argument.starts_with("--input-format="))
      config.inputFormat=argument=="--input-format"?
        next("--input-format"):argument.substr(sizeof("--input-format=" )-1);
    else if(argument=="--output-format"||
        argument.starts_with("--output-format="))
      config.outputFormat=argument=="--output-format"?
        next("--output-format"):argument.substr(sizeof("--output-format=" )-1);
    else if(argument=="--no-thinking")config.thinking=false;
    else if(argument=="--thinking")config.thinking=true;
    else if(argument=="--continue"||argument=="-c")config.continueFlight=true;
    else if(argument=="--resume"||argument=="-r"){
      // A missing value, or a following option, opens the picker instead.
      if(i+1==arguments.size()||arguments[i+1].starts_with('-'))config.resumePicker=true;
      else config.resumeFlight=arguments[++i];
    }
    else if(argument=="--mcp-config"){
      const auto value=readObject(runtime::pathFromUtf8(next("--mcp-config")),true);
      config.mcpServers=value.value("mcpServers",value);
      config.mcpServersConfigured=true;
    }else if(argument=="--max-output-tokens"){
      config.maxOutputTokens=positive(next("--max-output-tokens"),"--max-output-tokens");
      requestedOutput=config.maxOutputTokens;}
    else if(argument=="--context-window"){
      config.contextWindow=positive(next("--context-window"),"--context-window");
      requestedContext=config.contextWindow;}
    else if(argument=="--compact-at")config.compactAt=positive(next("--compact-at"),"--compact-at");
    else if(argument=="--max-subagents")config.maxSubagents=positive(next("--max-subagents"),"--max-subagents");
    else if(argument=="--max-concurrent-subagents")config.maxConcurrentSubagents=positive(next("--max-concurrent-subagents"),"--max-concurrent-subagents");
    else if(argument=="--"){for(++i;i<arguments.size();++i)poetry.push_back(arguments[i]);break;}
    else if(argument.starts_with('-'))throw std::invalid_argument("Unknown option: "+argument);
    else poetry.push_back(argument);
  }
  config.modelRequested=config.modelRequested||requestedModel.has_value();
  config.permissionModeRequested=requestedPermissionMode.has_value()||config.print;
  if(config.resumePicker&&config.print)
    throw std::invalid_argument("--resume needs a Flight id, slug or title with --print");
  if(config.resumePicker&&!poetry.empty())
    throw std::invalid_argument("--resume without a value opens the Flight picker and "
      "cannot take an initial prompt; use --resume <slug> \"prompt\"");
  if(requestedPermissionMode||config.print){
    if(!config.permissions.is_object())
      throw std::invalid_argument("Permissions must be a JSON object");
    const bool disabled=bypassPermissionsDisabled(config,config.settings);
    const auto selected=requestedPermissionMode.value_or(
      disabled?"dontAsk":"bypassPermissions");
    if(selected=="bypassPermissions"&&disabled)
      throw std::invalid_argument("bypassPermissions is disabled by settings or configuration");
    config.permissions["defaultMode"]=selected;
  }
  if(config.help){
    config.cwd=std::filesystem::absolute(config.cwd).lexically_normal();
    return config;
  }
  for(const auto& [id,endpoint]:config.endpoints)
    for(const auto& wireModel:endpoint.models){
      const auto name=application_config::profileId(id,wireModel);
      if(config.profiles.contains(name))throw std::invalid_argument(
        "Endpoint profile conflicts with an explicit profile: "+name);
      config.profiles.emplace(name,application_config::inferenceProfile(endpoint,wireModel));
    }
  if(!config.profiles.empty()){
    if(config.selectedProfile.empty()){
      if(requestedModel&&config.profiles.contains(*requestedModel)){
        config.selectedProfile=*requestedModel;
        requestedModel.reset();
      }else if(config.profiles.size()==1)
        config.selectedProfile=config.profiles.begin()->first;
      else if(config.profiles.contains(config.model))config.selectedProfile=config.model;
      else throw std::invalid_argument("Select a configured inference profile with --profile");
    }
    auto found=config.profiles.find(config.selectedProfile);
    if(found==config.profiles.end())throw std::invalid_argument(
      "Unknown inference profile: "+config.selectedProfile);
    auto& selected=found->second;
    if(requestedModel)selected.model=*requestedModel;
    if(requestedBaseUrl)selected.baseUrl=*requestedBaseUrl;
    if(requestedHelper)selected.apiKeyHelper=*requestedHelper;
    if(requestedToken)selected.authToken=*requestedToken;
    if(requestedContext)selected.contextWindow=*requestedContext;
    if(requestedOutput)selected.maxOutputTokens=*requestedOutput;
    config.model=config.selectedProfile;
    config.baseUrl=selected.baseUrl;
    config.authToken=selected.authToken;
    config.apiKeyHelper=selected.apiKeyHelper;
    config.contextWindow=selected.contextWindow;
    config.maxOutputTokens=selected.maxOutputTokens;
  }else if(!config.selectedProfile.empty())
    throw std::invalid_argument("--profile requires configured profiles");
  config.cwd=std::filesystem::absolute(config.cwd).lexically_normal();
  for(const auto& word:poetry){if(!config.initialPoetry)config.initialPoetry="";else *config.initialPoetry+=' ';*config.initialPoetry+=word;}
  if(config.inputFormat!="text"&&config.inputFormat!="stream-json")
    throw std::invalid_argument("--input-format must be text or stream-json");
  if(config.outputFormat!="text"&&config.outputFormat!="json"&&
      config.outputFormat!="stream-json")
    throw std::invalid_argument("--output-format must be text, json or stream-json");
  if(!config.print&&(config.inputFormat!="text"||config.outputFormat!="text"||
      config.replayUserMessages))
    throw std::invalid_argument("Input/output formats and replay require --print");
  if(config.inputFormat=="stream-json"&&config.outputFormat!="stream-json")
    throw std::invalid_argument("stream-json input requires stream-json output");
  if(config.outputFormat=="stream-json"&&!config.verbose)
    throw std::invalid_argument("--output-format=stream-json requires --verbose");
  if(config.replayUserMessages&&
      (config.inputFormat!="stream-json"||config.outputFormat!="stream-json"))
    throw std::invalid_argument("--replay-user-messages requires stream-json input and output");
  if(!config.mcpServers.is_object()||!config.permissions.is_object())throw std::invalid_argument("MCP servers and permissions must be JSON objects");
  for(const auto& [name,profile]:config.profiles){
    if(profile.model.empty()||profile.baseUrl.empty()||profile.messagesPath.empty()||
       profile.messagesPath.front()!='/')
      throw std::invalid_argument("Profile needs model, base_url, and absolute messages_path: "+name);
    if(profile.authHeader!="authorization"&&profile.authHeader!="x-api-key"&&
       profile.authHeader!="none")
      throw std::invalid_argument("Profile auth_header must be authorization, x-api-key, or none: "+name);
  }
  return config;
}
namespace {
std::string runCredentialHelper(const std::string& command){
  if(command.empty())return {};
#if defined(_WIN32)
  auto* process=_popen(command.c_str(),"r");
#else
  auto* process=popen(command.c_str(),"r");
#endif
  if(!process)throw std::runtime_error("Cannot start API credential helper");
  std::string token;std::array<char,4096> buffer{};
  while(const auto size=std::fread(buffer.data(),1,buffer.size(),process))token.append(buffer.data(),size);
#if defined(_WIN32)
  const auto status=_pclose(process);
#else
  const auto status=pclose(process);
#endif
  if(status!=0)throw std::runtime_error("API credential helper failed");
  return value::trim(token);
}
}
std::string resolveAuthToken(const Config& config){
  if(!config.authToken.empty())return config.authToken;
  return runCredentialHelper(config.apiKeyHelper);
}
InferenceProfile selectedInferenceProfile(const Config& config){
  if(!config.profiles.empty()){
    const auto found=config.profiles.find(config.model);
    if(found==config.profiles.end())throw std::invalid_argument(
      "Unknown inference profile: "+config.model);
    return found->second;
  }
  InferenceProfile profile;
  profile.model=config.model;profile.baseUrl=config.baseUrl;
  profile.authToken=config.authToken;profile.apiKeyHelper=config.apiKeyHelper;
  profile.contextWindow=config.contextWindow;
  profile.maxOutputTokens=config.maxOutputTokens;
  profile.thinking=true;
  profile.poetryStashing=true;
  profile.effort=true;
  profile.sendDisabledThinking=true;
  return profile;
}
std::string resolveProfileAuthToken(const Config& config,const std::string& name){
  if(config.profiles.empty())return resolveAuthToken(config);
  const auto found=config.profiles.find(name);
  if(found==config.profiles.end())throw std::invalid_argument(
    "Unknown inference profile: "+name);
  const auto& profile=found->second;
  if(!profile.authToken.empty())return profile.authToken;
  if(!profile.apiKeyEnv.empty()){
    const auto value=environment(profile.apiKeyEnv.c_str());
    if(!value.empty())return value;
  }
  return runCredentialHelper(profile.apiKeyHelper);
}
Json modelOptions(const Config& config){
  return {{"model",config.model},{"maxTokens",config.maxOutputTokens},
    {"contextWindow",config.contextWindow},{"querySource","repl_main_thread"}};
}
std::string commandLineHelp(){
  return "Batbox terminal\n\nUsage: batbox [options] [poetry]\n\n"
    "  --fullscreen | --inline       Terminal layout\n"
    "  -p, --print                    Run Gadgets without approval dialogs, print and exit\n"
    "  --input-format FORMAT          text or stream-json (with --print)\n"
    "  --output-format FORMAT         text, json or stream-json (with --print)\n"
    "  --verbose                      Required for stream-json output\n"
    "  --replay-user-messages         Echo stream-json input messages\n"
    "  --model MODEL                  Inference model\n"
    "  --profile NAME                 Named Messages API profile\n"
    "  --base-url URL                 Messages API server\n"
    "  --config PATH                  Client configuration\n"
    "  --cwd PATH                     Working directory\n"
    "  --permission-mode MODE         Permission mode for this Flight (also with --print)\n"
    "  --yolo                         Skip Gadget approval dialogs for this Flight and its Bats\n"
    "  --dangerously-skip-permissions  Alias for --yolo\n"
    "                                 default, acceptEdits, bypassPermissions, dontAsk, plan\n"
    "                                 --print defaults to bypassPermissions; policy still applies\n"
    "  --mcp-config PATH              MCP server configuration\n"
    "  -c, --continue                 Resume the most recent Flight for this directory\n"
    "                                 and its git worktrees\n"
    "  -r, --resume [FLIGHT]          Resume a saved Flight; FLIGHT is an id, slug or\n"
    "                                 title words. Without FLIGHT, pick from a list\n"
    "  --max-output-tokens N          Output limit\n"
    "  --context-window N             Model context capacity\n"
    "  --thinking | --no-thinking     Extended model reasoning\n"
    "  --compact-at N                 Automatic compaction threshold\n"
    "  --max-subagents N              Subagent limit\n"
    "  --max-concurrent-subagents N   Concurrent subagents\n\n"
    "Configuration: ~/.batbox/native-client.json and ~/.batbox/settings.json\n"
    "Bats: ~/.batbox/agents/*.md; skills: ~/.batbox/skills/\n"
    "MCP: ~/.batbox/mcp.json; BATBOX_CONFIG_DIR overrides the configuration root.\n";
}
} // namespace batbox::application
