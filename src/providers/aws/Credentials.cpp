#include "providers/aws/Credentials.hpp"
#include "providers/cloud/HttpExchange.hpp"
#include "runtime/JsonValue.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace batbox::providers::aws {
namespace {
std::string env(const char* name) {
  const char* value = std::getenv(name);
  return value ? std::string(value) : std::string();
}
std::string readFile(const std::filesystem::path& path) {
  std::ifstream input(path,std::ios::binary);
  if (!input) return {};
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}
std::string lower(std::string text) {
  for (auto& c : text) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c+('a'-'A'));
  return text;
}
std::string get(const std::map<std::string,std::string>& map, const std::string& key) {
  const auto it = map.find(key);
  return it == map.end() ? std::string() : it->second;
}
std::string xmlField(const std::string& xml, const std::string& tag) {
  const auto open = "<"+tag+">";
  const auto start = xml.find(open);
  if (start == std::string::npos) return {};
  const auto end = xml.find("</"+tag+">",start);
  if (end == std::string::npos) return {};
  return cloud::trimmed(xml.substr(start+open.size(),end-start-open.size()));
}
std::string stsEndpoint(const std::string& region) {
  if (auto url = env("AWS_ENDPOINT_URL_STS"); !url.empty()) return url;
  if (auto url = env("AWS_ENDPOINT_URL"); !url.empty()) return url;
  return region.empty() ? "https://sts.amazonaws.com" : "https://sts."+region+".amazonaws.com";
}
} // namespace
IniFile parseIni(std::string_view text) {
  IniFile result;
  std::string section;
  bool inSection = false;
  std::size_t start = 0;
  while (start <= text.size()) {
    auto end = text.find('\n',start);
    if (end == std::string_view::npos) end = text.size();
    std::string line(text.substr(start,end-start));
    start = end+1;
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const bool indented = !line.empty() && (line.front() == ' ' || line.front() == '\t');
    line = cloud::trimmed(line);
    if (line.empty() || line.front() == '#' || line.front() == ';') continue;
    if (line.front() == '[' && line.back() == ']') {
      section = cloud::trimmed(line.substr(1,line.size()-2));
      inSection = true;
      result[section];
      continue;
    }
    // Indented lines belong to nested settings such as "s3 =", which Bedrock does not use.
    if (!inSection || indented) continue;
    const auto equals = line.find('=');
    if (equals == std::string::npos) continue;
    result[section][lower(cloud::trimmed(line.substr(0,equals)))] = cloud::trimmed(line.substr(equals+1));
  }
  return result;
}
std::filesystem::path homeDirectory() {
  if (auto home = env("HOME"); !home.empty()) return home;
  if (auto profile = env("USERPROFILE"); !profile.empty()) return profile;
  return {};
}
std::filesystem::path sharedConfigPath() {
  if (auto path = env("AWS_CONFIG_FILE"); !path.empty()) return path;
  return homeDirectory()/".aws"/"config";
}
std::filesystem::path sharedCredentialsPath() {
  if (auto path = env("AWS_SHARED_CREDENTIALS_FILE"); !path.empty()) return path;
  return homeDirectory()/".aws"/"credentials";
}
std::optional<Credentials> parseProcessCredentials(std::string_view output, std::string& error) {
  const auto json = value::parse(output,false);
  if (json.is_discarded() || !json.is_object()) { error = "credential_process did not print a JSON object"; return std::nullopt; }
  if (!json.contains("Version") || !json["Version"].is_number_integer() || json["Version"].get<int>() != 1) {
    error = "credential_process output must have \"Version\": 1";
    return std::nullopt;
  }
  Credentials credentials;
  credentials.accessKeyId = json.value("AccessKeyId","");
  credentials.secretAccessKey = json.value("SecretAccessKey","");
  credentials.sessionToken = json.value("SessionToken","");
  if (credentials.empty()) { error = "credential_process output lacks AccessKeyId or SecretAccessKey"; return std::nullopt; }
  if (json.contains("Expiration") && json["Expiration"].is_string()) {
    credentials.expiration = cloud::parseTimestamp(json["Expiration"].get<std::string>());
    if (!credentials.expiration) { error = "credential_process Expiration is not an ISO 8601 timestamp"; return std::nullopt; }
  }
  return credentials;
}
CredentialChain::CredentialChain(CredentialOptions settings) : options(std::move(settings)) {}
std::string CredentialChain::profileName() const {
  if (!options.profile.empty()) return options.profile;
  if (auto profile = env("AWS_PROFILE"); !profile.empty()) return profile;
  if (auto profile = env("AWS_DEFAULT_PROFILE"); !profile.empty()) return profile;
  return "default";
}
std::map<std::string,std::string> CredentialChain::profileSettings(const std::string& name) const {
  std::map<std::string,std::string> settings;
  const auto config = parseIni(readFile(sharedConfigPath()));
  const auto pick = [&](const IniFile& file, const std::string& section) {
    if (const auto it = file.find(section); it != file.end())
      for (const auto& [key,value] : it->second) settings[key] = value;
  };
  if (name == "default") pick(config,"default");
  pick(config,"profile "+name);
  pick(parseIni(readFile(sharedCredentialsPath())),name);
  return settings;
}
std::string CredentialChain::region() const {
  if (auto region = env("AWS_REGION"); !region.empty()) return region;
  if (auto region = env("AWS_DEFAULT_REGION"); !region.empty()) return region;
  return get(profileSettings(profileName()),"region");
}
bool CredentialChain::fresh(const std::optional<std::chrono::system_clock::time_point>& expiration) const {
  return !expiration || options.now()+options.refreshMargin < *expiration;
}
Credentials CredentialChain::runCredentialProcess(const std::string& command) {
  if (const auto it = processStash.find(command); it != processStash.end() && fresh(it->second.expiration)) return it->second;
  const auto result = options.runCommand(command);
  if (result.status != 0) throw std::runtime_error("credential_process exited with status "+std::to_string(result.status));
  std::string error;
  auto credentials = parseProcessCredentials(result.output,error);
  if (!credentials) throw std::runtime_error(error);
  processStash[command] = *credentials;
  return *credentials;
}
Credentials CredentialChain::assumeRole(const std::map<std::string,std::string>& settings, const Credentials& sourceCredentials) {
  if (!get(settings,"mfa_serial").empty()) throw std::runtime_error("Profiles with mfa_serial need an interactive MFA code; use credential_process instead");
  const auto region = get(settings,"region");
  const auto endpoint = stsEndpoint(region);
  std::vector<std::pair<std::string,std::string>> form{{"Action","AssumeRole"},{"Version","2011-06-15"},
    {"RoleArn",get(settings,"role_arn")}};
  auto sessionName = get(settings,"role_session_name");
  if (sessionName.empty()) sessionName = "batbox-"+std::to_string(std::chrono::duration_cast<std::chrono::seconds>(options.now().time_since_epoch()).count());
  form.emplace_back("RoleSessionName",sessionName);
  if (auto duration = get(settings,"duration_seconds"); !duration.empty()) form.emplace_back("DurationSeconds",duration);
  if (auto external = get(settings,"external_id"); !external.empty()) form.emplace_back("ExternalId",external);
  const auto url = cloud::joinUrl(endpoint,"/");
  const auto parts = cloud::splitUrl(url);
  SigningRequest request;
  request.method = "POST";
  request.target = parts.target;
  request.body = cloud::formEncode(form);
  request.headers = {{"host",parts.host},{"content-type","application/x-www-form-urlencoded; charset=utf-8"}};
  SigningOptions signing;
  signing.region = region.empty() ? "us-east-1" : region;
  signing.service = "sts";
  signing.time = options.now();
  const auto signed_ = sign(request,sourceCredentials,signing);
  cloud::HttpRequest http;
  http.url = url;
  http.body = request.body;
  http.timeout = std::chrono::seconds(30);
  for (const auto& header : signed_.headers) if (header.first != "host") http.headers.push_back(header);
  cloud::HttpExchange exchange(options.network);
  const auto response = exchange.perform(http);
  if (response.status != 200) {
    auto message = xmlField(response.body,"Message");
    throw std::runtime_error("STS AssumeRole failed ("+std::to_string(response.status)+" "+xmlField(response.body,"Code")+")"+
                             (message.empty() ? "" : ": "+message));
  }
  Credentials credentials;
  credentials.accessKeyId = xmlField(response.body,"AccessKeyId");
  credentials.secretAccessKey = xmlField(response.body,"SecretAccessKey");
  credentials.sessionToken = xmlField(response.body,"SessionToken");
  credentials.expiration = cloud::parseTimestamp(xmlField(response.body,"Expiration"));
  if (credentials.empty()) throw std::runtime_error("STS AssumeRole response did not include credentials");
  return credentials;
}
Credentials CredentialChain::resolveProfile(const std::string& name, std::set<std::string>& visiting, std::string& source) {
  if (!visiting.insert(name).second) throw std::runtime_error("AWS profile "+name+" has a source_profile cycle");
  const auto settings = profileSettings(name);
  if (settings.empty()) throw std::runtime_error("AWS profile "+name+" was not found in "+sharedCredentialsPath().string()+" or "+sharedConfigPath().string());
  Credentials staticKeys{get(settings,"aws_access_key_id"),get(settings,"aws_secret_access_key"),get(settings,"aws_session_token"),{}};
  if (!get(settings,"role_arn").empty()) {
    Credentials sourceCredentials;
    const auto sourceProfile = get(settings,"source_profile");
    const auto credentialSource = get(settings,"credential_source");
    if (!sourceProfile.empty()) {
      if (sourceProfile == name) {
        if (staticKeys.empty()) throw std::runtime_error("AWS profile "+name+" sources itself but has no static keys");
        sourceCredentials = staticKeys;
      } else {
        std::string ignored;
        sourceCredentials = resolveProfile(sourceProfile,visiting,ignored);
      }
    } else if (credentialSource == "Environment") {
      sourceCredentials = {env("AWS_ACCESS_KEY_ID"),env("AWS_SECRET_ACCESS_KEY"),env("AWS_SESSION_TOKEN"),{}};
      if (sourceCredentials.empty()) throw std::runtime_error("AWS profile "+name+" uses credential_source = Environment but AWS_ACCESS_KEY_ID is not set");
    } else {
      throw std::runtime_error("AWS profile "+name+" has role_arn without a supported source_profile or credential_source");
    }
    source = "profile "+name+" (assume role)";
    return assumeRole(settings,sourceCredentials);
  }
  if (!staticKeys.empty()) { source = "profile "+name; return staticKeys; }
  if (const auto command = get(settings,"credential_process"); !command.empty()) {
    source = "profile "+name+" (credential_process)";
    return runCredentialProcess(command);
  }
  if (!get(settings,"sso_session").empty() || !get(settings,"sso_start_url").empty())
    throw std::runtime_error("AWS profile "+name+" uses SSO, which Batbox reads through credential_process: "
                             "credential_process = aws configure export-credentials --profile "+name+" --format process");
  throw std::runtime_error("AWS profile "+name+" has no credentials");
}
CredentialChain::Stashed CredentialChain::resolveUncached() {
  Stashed stashed;
  auto& auth = stashed.auth;
  if (!options.bearerToken.empty()) {
    auth.kind = BedrockAuth::Kind::Bearer;
    auth.bearerToken = options.bearerToken;
    auth.source = "endpoint auth_token";
    return stashed;
  }
  if (auto bearer = env("AWS_BEARER_TOKEN_BEDROCK"); !bearer.empty()) {
    auth.kind = BedrockAuth::Kind::Bearer;
    auth.bearerToken = bearer;
    auth.source = "AWS_BEARER_TOKEN_BEDROCK";
    return stashed;
  }
  auth.credentials = {env("AWS_ACCESS_KEY_ID"),env("AWS_SECRET_ACCESS_KEY"),env("AWS_SESSION_TOKEN"),{}};
  if (auth.credentials.sessionToken.empty()) auth.credentials.sessionToken = env("AWS_SECURITY_TOKEN");
  if (!auth.credentials.empty()) { auth.source = "environment"; return stashed; }
  std::set<std::string> visiting;
  const auto profile = profileName();
  try {
    auth.credentials = resolveProfile(profile,visiting,auth.source);
  } catch (const std::exception& error) {
    throw std::runtime_error(std::string("No usable AWS credentials. Set AWS_BEARER_TOKEN_BEDROCK, AWS_ACCESS_KEY_ID and "
                             "AWS_SECRET_ACCESS_KEY, or AWS_PROFILE. ")+error.what());
  }
  stashed.expiration = auth.credentials.expiration;
  return stashed;
}
BedrockAuth CredentialChain::resolve() {
  std::lock_guard lock(mutex);
  if (stash && fresh(stash->expiration)) return stash->auth;
  stash = resolveUncached();
  return stash->auth;
}
void CredentialChain::invalidate() {
  std::lock_guard lock(mutex);
  stash.reset();
  processStash.clear();
}
} // namespace batbox::providers::aws
