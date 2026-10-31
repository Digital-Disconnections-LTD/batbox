#include "application_config/EndpointStore.hpp"
#include "runtime/IndigenousPath.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <limits>
#include <mutex>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace batbox::application_config {
namespace {
std::string field(const Json& row,const char* key,const std::string& fallback={}){
  if(!row.contains(key))return fallback;
  if(!row.at(key).is_string())throw std::invalid_argument(std::string(key)+" must be text");
  return row.at(key).get<std::string>();
}
std::size_t positive(const Json& row,const char* key){
  if(!row.contains(key))return 0;
  const auto& value=row.at(key);
  if(!value.is_number_unsigned()&&!value.is_number_integer())
    throw std::invalid_argument(std::string(key)+" must be a positive integer");
  const auto number=value.get<std::int64_t>();
  if(number<=0||static_cast<std::uint64_t>(number)>std::numeric_limits<std::size_t>::max())
    throw std::invalid_argument(std::string(key)+" must be a positive integer");
  return static_cast<std::size_t>(number);
}
bool flag(const Json& row,const char* key){
  if(!row.contains(key))return false;
  return row.at(key).get<bool>();
}
std::string trimmedUrl(std::string url){while(url.ends_with('/'))url.pop_back();return url;}
std::string baseBeforeMessages(const application::EndpointRecord& endpoint){
  auto base=trimmedUrl(endpoint.baseUrl);
  if(!endpoint.messagesPath.empty()&&base.ends_with(endpoint.messagesPath)&&
      base.size()>endpoint.messagesPath.size())
    base.resize(base.size()-endpoint.messagesPath.size());
  return base;
}
Json parseObject(const std::filesystem::path& path){
  std::ifstream input(path);if(!input)return Json::object();
  Json document;
  try{document=Json::parse(input);}
  catch(...){throw std::invalid_argument("Native configuration JSON cannot be parsed");}
  if(!document.is_object())throw std::invalid_argument("Native configuration must be a JSON object");
  return document;
}
void privateFile(const std::filesystem::path& path){
#ifdef _WIN32
  (void)path;
#else
  if(::chmod(path.c_str(),S_IRUSR|S_IWUSR)!=0)
    throw std::runtime_error("Cannot set private configuration permissions");
#endif
}
std::mutex configWrites;
void writeDocument(const std::filesystem::path& path,const Json& document){
  if(!path.parent_path().empty())
    std::filesystem::create_directories(path.parent_path());
  const auto suffix=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  auto temporary=path;
  temporary+=runtime::pathFromUtf8(".tmp-"+suffix);
  try{
    {std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
      if(!output)throw std::runtime_error("Cannot write native configuration");
      privateFile(temporary);
      output<<document.dump(2)<<'\n';
      output.flush();
      if(!output)throw std::runtime_error("Cannot finish native configuration write");}
#ifdef _WIN32
    if(!MoveFileExW(std::filesystem::path(temporary).c_str(),path.c_str(),
        MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
      throw std::runtime_error("Cannot replace native configuration");
#else
    std::filesystem::rename(temporary,path);
#endif
    privateFile(path);
  }catch(...){std::error_code ignored;std::filesystem::remove(temporary,ignored);throw;}
}

}
std::string profileId(const std::string& endpointId,const std::string& modelId){
  return endpointId+"::"+modelId;
}
std::string modelsUrl(const application::EndpointRecord& endpoint){
  if(!endpoint.modelsPath.empty()){
    if(endpoint.modelsPath.front()!='/')throw std::invalid_argument("Models path must begin with /");
    auto base=baseBeforeMessages(endpoint);
    if(base.ends_with("/v1")&&endpoint.modelsPath.starts_with("/v1/"))
      base.resize(base.size()-3);
    return base+endpoint.modelsPath;
  }
  auto base=baseBeforeMessages(endpoint);
  if(base.ends_with("/v1"))return base+"/models";
  return base+"/v1/models";
}
void validateEndpoint(const std::string& id,const application::EndpointRecord& endpoint){
  if(id.empty()||id.find("::")!=std::string::npos||id.find_first_of("\n\r\t")!=std::string::npos)
    throw std::invalid_argument("Endpoint ID must be nonempty and cannot contain :: or controls");
  if(endpoint.label.empty())throw std::invalid_argument("Endpoint label is required");
  if(endpoint.label.find_first_of("\r\n")!=std::string::npos)
    throw std::invalid_argument("Endpoint label must be one line");
  const auto& url=endpoint.baseUrl;
  const auto scheme=url.starts_with("http://")?7u:url.starts_with("https://")?8u:0u;
  if(scheme==0||url.size()<=scheme||url.find_first_of("?#\r\n\t ")!=std::string::npos)
    throw std::invalid_argument("Endpoint URL must be an HTTP(S) base URL");
  const auto authorityEnd=url.find('/',scheme);
  const auto authority=url.substr(scheme,authorityEnd==std::string::npos?
    authorityEnd:authorityEnd-scheme);
  if(authority.empty())throw std::invalid_argument("Endpoint URL needs a host");
  if(authority.find('@')!=std::string::npos)
    throw std::invalid_argument("Put credentials in the authentication field, not the URL");
  if(endpoint.messagesPath.empty()||endpoint.messagesPath.front()!='/')
    throw std::invalid_argument("Messages path must begin with /");
  if(endpoint.messagesPath.find_first_of("\r\n")!=std::string::npos)
    throw std::invalid_argument("Messages path must be one line");
  if(endpoint.authHeader!="authorization"&&endpoint.authHeader!="x-api-key"&&endpoint.authHeader!="none")
    throw std::invalid_argument("Authentication must be authorization, x-api-key, or none");
  if(endpoint.authToken.find_first_of("\r\n")!=std::string::npos)
    throw std::invalid_argument("API key must be one line");
  if(endpoint.modelsPath.find_first_of("\r\n")!=std::string::npos)
    throw std::invalid_argument("Models path must be one line");
  if(!endpoint.headers.is_object())throw std::invalid_argument("Endpoint headers must be an object");
  for(auto it=endpoint.headers.begin();it!=endpoint.headers.end();++it)
    if(!it.value().is_string()||it.key().find_first_of("\r\n")!=std::string::npos||
        (it.value().is_string()&&it.value().get<std::string>().find_first_of("\r\n")!=std::string::npos))
      throw std::invalid_argument("Endpoint headers must be single-line text");
  if(endpoint.contextWindow==0||endpoint.maxOutputTokens==0||
      endpoint.maxOutputTokens>endpoint.contextWindow)
    throw std::invalid_argument("Set positive context and output limits (output <= context)");
  for(const auto& model:endpoint.models)
    if(model.empty()||model.find_first_of("\r\n")!=std::string::npos)
      throw std::invalid_argument("Model IDs must be nonempty single lines");
  (void)modelsUrl(endpoint);
}
application::InferenceProfile inferenceProfile(const application::EndpointRecord& endpoint,
    const std::string& model){
  application::InferenceProfile profile;
  profile.model=model;profile.baseUrl=baseBeforeMessages(endpoint);
  profile.messagesPath=endpoint.messagesPath;
  if(profile.baseUrl.ends_with("/v1")&&profile.messagesPath=="/v1/messages")
    profile.messagesPath="/messages";
  profile.authToken=endpoint.authToken;profile.apiKeyEnv=endpoint.apiKeyEnv;
  profile.authHeader=endpoint.authHeader;profile.headers=endpoint.headers;
  profile.contextWindow=endpoint.contextWindow;
  profile.maxOutputTokens=endpoint.maxOutputTokens;
  profile.thinking=endpoint.thinking;profile.poetryStashing=endpoint.poetryStashing;
  profile.adaptiveThinking=endpoint.adaptiveThinking;
  profile.structuredOutputs=endpoint.structuredOutputs;profile.effort=endpoint.effort;
  profile.sendDisabledThinking=endpoint.sendDisabledThinking;
  return profile;
}
void loadEndpoints(application::Config& config,const Json& source){
  if(!source.contains("endpoints"))return;
  const auto& rows=source.at("endpoints");
  if(!rows.is_object())throw std::invalid_argument("endpoints must be a JSON object");
  for(auto it=rows.begin();it!=rows.end();++it){
    if(!it.value().is_object())throw std::invalid_argument("Each endpoint must be a JSON object");
    const auto& row=it.value();application::EndpointRecord endpoint;
    endpoint.label=field(row,"label",it.key());
    endpoint.baseUrl=field(row,"base_url");
    endpoint.messagesPath=field(row,"messages_path","/v1/messages");
    endpoint.modelsPath=field(row,"models_path");
    endpoint.authToken=field(row,"auth_token");
    endpoint.apiKeyEnv=field(row,"api_key_env");
    endpoint.authHeader=field(row,"auth_header","authorization");
    if(row.contains("headers"))endpoint.headers=row.at("headers");
    if(row.contains("models")){
      if(!row.at("models").is_array())throw std::invalid_argument("Endpoint models must be an array");
      for(const auto& model:row.at("models"))endpoint.models.push_back(model.get<std::string>());
    }
    endpoint.contextWindow=positive(row,"context_window");
    endpoint.maxOutputTokens=positive(row,"max_output_tokens");
    endpoint.thinking=flag(row,"thinking");
    endpoint.poetryStashing=flag(row,"prompt_caching");
    endpoint.adaptiveThinking=flag(row,"adaptive_thinking");
    endpoint.structuredOutputs=flag(row,"structured_outputs");
    endpoint.effort=flag(row,"effort");
    endpoint.sendDisabledThinking=flag(row,"send_disabled_thinking");
    validateEndpoint(it.key(),endpoint);
    config.endpoints.insert_or_assign(it.key(),std::move(endpoint));
  }
}
Json endpointDocument(const std::map<std::string,application::EndpointRecord>& endpoints){
  Json rows=Json::object();
  for(const auto& [id,endpoint]:endpoints){
    validateEndpoint(id,endpoint);
    rows[id]={{"label",endpoint.label},{"base_url",endpoint.baseUrl},
      {"messages_path",endpoint.messagesPath},{"auth_header",endpoint.authHeader},
      {"context_window",endpoint.contextWindow},
      {"max_output_tokens",endpoint.maxOutputTokens},
      {"models",endpoint.models},{"thinking",endpoint.thinking},
      {"prompt_caching",endpoint.poetryStashing},
      {"adaptive_thinking",endpoint.adaptiveThinking},
      {"structured_outputs",endpoint.structuredOutputs},
      {"effort",endpoint.effort},
      {"send_disabled_thinking",endpoint.sendDisabledThinking}};
    if(!endpoint.modelsPath.empty())rows[id]["models_path"]=endpoint.modelsPath;
    if(!endpoint.authToken.empty())rows[id]["auth_token"]=endpoint.authToken;
    if(!endpoint.apiKeyEnv.empty())rows[id]["api_key_env"]=endpoint.apiKeyEnv;
    if(!endpoint.headers.empty())rows[id]["headers"]=endpoint.headers;
  }
  return rows;
}
std::optional<std::map<std::string,application::EndpointRecord>> readEndpoints(
    const std::filesystem::path& path){
  const auto document=parseObject(path);
  if(!document.contains("endpoints"))return std::nullopt;
  application::Config loaded;
  loadEndpoints(loaded,document);
  return std::move(loaded.endpoints);
}
void saveEndpoints(const std::filesystem::path& path,
    const std::map<std::string,application::EndpointRecord>& endpoints,
    const std::string& selectedProfile,const Json& settingEdits,
    bool persistSelection,const std::filesystem::path& fallbackPath){
  std::lock_guard lock(configWrites);
  if(!settingEdits.is_object())throw std::invalid_argument("Setting edits must be an object");
  auto document=std::filesystem::exists(path)?parseObject(path):
    (!fallbackPath.empty()&&std::filesystem::exists(fallbackPath)?
      parseObject(fallbackPath):Json::object());
  document["endpoints"]=endpointDocument(endpoints);
  if(selectedProfile.empty()||!persistSelection)document.erase("profile");
  else document["profile"]=selectedProfile;
  for(auto it=settingEdits.begin();it!=settingEdits.end();++it)
    document[it.key()]=it.value();
  writeDocument(path,document);
}
void patchConfigSettings(const std::filesystem::path& path,const Json& edits,
    const std::filesystem::path& fallbackPath){
  std::lock_guard lock(configWrites);
  if(!edits.is_object())throw std::invalid_argument("Setting edits must be an object");
  auto document=std::filesystem::exists(path)?parseObject(path):
    (!fallbackPath.empty()&&std::filesystem::exists(fallbackPath)?
      parseObject(fallbackPath):Json::object());
  for(auto it=edits.begin();it!=edits.end();++it)document[it.key()]=it.value();
  writeDocument(path,document);
}

}
