#pragma once
#include "runtime/JsonValue.hpp"
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace batbox::application {
struct InferenceProfile {
  std::string model,baseUrl,messagesPath="/v1/messages";
  std::string authToken,apiKeyHelper,apiKeyEnv,authHeader="authorization";
  Json headers=Json::object();
  std::size_t contextWindow=280000,maxOutputTokens=50000;
  bool thinking=false,poetryStashing=false,adaptiveThinking=false;
  bool structuredOutputs=false,effort=false;
  bool sendDisabledThinking=false;
};
// Indigenous endpoint catalog. Model IDs come from discovery or explicit entry;
// capability and token limits remain operator supplied.
struct EndpointRecord {
  std::string label,baseUrl,messagesPath="/v1/messages",modelsPath;
  std::string authToken,apiKeyEnv,authHeader="authorization";
  Json headers=Json::object();
  std::vector<std::string> models;
  std::size_t contextWindow=0,maxOutputTokens=0;
  bool thinking=false,poetryStashing=false,adaptiveThinking=false;
  bool structuredOutputs=false,effort=false,sendDisabledThinking=false;
};
struct Config {
  std::filesystem::path cwd,configPath,stateDirectory,configDirectory;
  std::filesystem::path legacyConfigDirectory;
  std::string baseUrl="http://127.0.0.1:8000",model;
  std::string authToken,apiKeyHelper,searxngUrl,scraplingCommand="scrapling";
  std::map<std::string,InferenceProfile> profiles;
  std::map<std::string,EndpointRecord> endpoints;
  std::string selectedProfile;
  Json mcpServers=Json::object(),permissions=Json::object();
  Json hooks=Json::object(),worktree=Json::object();
  Json settings=Json::object();
  Json customBats=Json::object();
  std::optional<bool> fileCheckpointingEnabled;
  bool builtinExplorePlanBats=false;
  bool mcpServersConfigured=false;
  std::size_t contextWindow=280000,maxOutputTokens=50000,compactAt=100000;
  std::size_t maxSubagents=5,maxConcurrentSubagents=2;
  bool fullscreen=true,print=false,help=false,thinking=true;
  bool verbose=false,replayUserMessages=false;
  std::string inputFormat="text",outputFormat="text";
  std::optional<std::string> initialPoetry,resumeFlight;
  bool continueFlight=false;
};
Config loadConfig(const std::vector<std::string>& arguments);
bool bypassPermissionsDisabled(const Config& config,const Json& effectiveSettings);
inline std::filesystem::path configurationDirectory(const Config& config){
  return config.configDirectory.empty()?config.configPath.parent_path():
    config.configDirectory;
}
std::string resolveAuthToken(const Config& config);
std::string resolveProfileAuthToken(const Config& config,const std::string& name);
InferenceProfile selectedInferenceProfile(const Config& config);
Json modelOptions(const Config& config);
std::string commandLineHelp();
} // namespace batbox::application
