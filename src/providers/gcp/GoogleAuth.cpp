#include "providers/gcp/GoogleAuth.hpp"
#include "providers/cloud/Crypto.hpp"
#include "providers/cloud/HttpExchange.hpp"
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace batbox::providers::gcp {
namespace {
std::string env(const char* name) {
  const char* value = std::getenv(name);
  return value ? std::string(value) : std::string();
}
std::optional<Json> readJson(const std::filesystem::path& path) {
  std::ifstream input(path,std::ios::binary);
  if (!input) return std::nullopt;
  std::ostringstream buffer;
  buffer << input.rdbuf();
  auto json = value::parse(buffer.str(),false);
  if (json.is_discarded() || !json.is_object()) throw std::runtime_error("Google credentials file is not a JSON object: "+path.string());
  return json;
}
long long epochSeconds(std::chrono::system_clock::time_point time) {
  return std::chrono::duration_cast<std::chrono::seconds>(time.time_since_epoch()).count();
}
std::string credentialsFilePath(const GoogleAuthOptions& options) {
  return options.credentialsFile.empty() ? env("GOOGLE_APPLICATION_CREDENTIALS") : options.credentialsFile;
}
} // namespace
std::string serviceAccountAssertion(const Json& account, std::chrono::system_clock::time_point now) {
  const auto email = account.value("client_email","");
  const auto key = account.value("private_key","");
  if (email.empty() || key.empty()) throw std::runtime_error("Service-account JSON lacks client_email or private_key");
  Json header{{"alg","RS256"},{"typ","JWT"}};
  if (const auto id = account.value("private_key_id",""); !id.empty()) header["kid"] = id;
  const auto issued = epochSeconds(now);
  Json claims{{"iss",email},{"scope",cloudPlatformScope},{"aud",account.value("token_uri","https://oauth2.googleapis.com/token")},
              {"iat",issued},{"exp",issued+3600}};
  const auto signingInput = cloud::base64UrlEncode(value::stringify(header))+"."+cloud::base64UrlEncode(value::stringify(claims));
  return signingInput+"."+cloud::base64UrlEncode(cloud::rs256Sign(key,signingInput));
}
std::filesystem::path wellKnownCredentialsPath() {
  if (auto config = env("CLOUDSDK_CONFIG"); !config.empty()) return std::filesystem::path(config)/"application_default_credentials.json";
#if defined(_WIN32)
  if (auto appData = env("APPDATA"); !appData.empty()) return std::filesystem::path(appData)/"gcloud"/"application_default_credentials.json";
#endif
  auto home = env("HOME");
  if (home.empty()) return {};
  return std::filesystem::path(home)/".config"/"gcloud"/"application_default_credentials.json";
}
GoogleTokenSource::GoogleTokenSource(GoogleAuthOptions settings) : options(std::move(settings)) {}
AccessToken GoogleTokenSource::exchange(const std::string& tokenUri, const std::string& form, const std::string& source) {
  cloud::HttpExchange http(options.network);
  cloud::HttpRequest request;
  request.url = tokenUri;
  request.body = form;
  request.timeout = std::chrono::seconds(30);
  request.headers = {{"content-type","application/x-www-form-urlencoded"},{"accept","application/json"},{"user-agent","batbox/0.0.1"}};
  const auto result = http.perform(request);
  const auto json = value::parse(result.body,false);
  if (result.status != 200) {
    std::string detail = json.is_object() ? json.value("error_description",json.value("error","")) : std::string();
    throw std::runtime_error("Google token exchange failed ("+std::to_string(result.status)+")"+(detail.empty() ? "" : ": "+detail));
  }
  AccessToken token;
  token.token = json.is_object() ? json.value("access_token","") : std::string();
  if (token.token.empty()) throw std::runtime_error("Google token response had no access_token");
  if (json.contains("expires_in") && json["expires_in"].is_number())
    token.expiration = options.now()+std::chrono::seconds(json["expires_in"].get<long long>());
  token.source = source;
  return token;
}
AccessToken GoogleTokenSource::fromCredentialsFile(const std::filesystem::path& path) {
  const auto json = readJson(path);
  if (!json) throw std::runtime_error("Cannot read Google credentials file "+path.string());
  const auto type = json->value("type","");
  if (type == "service_account") {
    const auto tokenUri = json->value("token_uri","https://oauth2.googleapis.com/token");
    const auto form = cloud::formEncode({{"grant_type","urn:ietf:params:oauth:grant-type:jwt-bearer"},
                                         {"assertion",serviceAccountAssertion(*json,options.now())}});
    return exchange(tokenUri,form,"service account "+json->value("client_email",""));
  }
  if (type == "authorized_user") {
    const auto tokenUri = json->value("token_uri","https://oauth2.googleapis.com/token");
    const auto form = cloud::formEncode({{"grant_type","refresh_token"},{"client_id",json->value("client_id","")},
                                         {"client_secret",json->value("client_secret","")},{"refresh_token",json->value("refresh_token","")}});
    return exchange(tokenUri,form,"application-default user credentials");
  }
  throw std::runtime_error("Unsupported Google credentials type \""+type+"\" in "+path.string()+
                           "; use a service_account or authorized_user file, or gcloud");
}
std::optional<AccessToken> GoogleTokenSource::fromGcloud() {
  if (options.gcloudCommand.empty()) return std::nullopt;
  cloud::CommandResult result;
  try { result = options.runCommand(options.gcloudCommand); }
  catch (const std::exception&) { return std::nullopt; }
  const auto token = cloud::trimmed(result.output);
  if (result.status != 0 || token.empty() || token.find_first_of(" \n") != std::string::npos) return std::nullopt;
  return AccessToken{token,options.now()+options.gcloudLifetime,"gcloud"};
}
std::optional<AccessToken> GoogleTokenSource::fromMetadata() {
  auto host = env("GCE_METADATA_HOST");
  if (host.empty()) host = "169.254.169.254";
  auto network = options.network;
  network.connectTimeout = std::chrono::milliseconds(1000);
  network.proxyUrl.clear();
  cloud::HttpExchange http(network);
  cloud::HttpRequest request;
  request.method = "GET";
  request.url = (host.find("://") == std::string::npos ? "http://"+host : host)+
                "/computeMetadata/v1/instance/service-accounts/default/token";
  request.timeout = std::chrono::seconds(3);
  request.headers = {{"metadata-flavor","Google"}};
  cloud::HttpResult result;
  try { result = http.perform(request); }
  catch (const std::exception&) { return std::nullopt; }
  if (result.status != 200) return std::nullopt;
  const auto json = value::parse(result.body,false);
  if (!json.is_object() || !json.contains("access_token")) return std::nullopt;
  AccessToken token{json.value("access_token",""),std::nullopt,"metadata server"};
  if (json.contains("expires_in") && json["expires_in"].is_number())
    token.expiration = options.now()+std::chrono::seconds(json["expires_in"].get<long long>());
  return token;
}
AccessToken GoogleTokenSource::fetch() {
  if (const auto path = credentialsFilePath(options); !path.empty()) return fromCredentialsFile(path);
  if (const auto path = wellKnownCredentialsPath(); !path.empty() && std::filesystem::exists(path)) return fromCredentialsFile(path);
  if (auto token = fromGcloud()) return *token;
  if (auto token = fromMetadata()) return *token;
  throw std::runtime_error("No Google credentials found. Set GOOGLE_APPLICATION_CREDENTIALS to a service-account JSON file, "
                           "run `gcloud auth application-default login`, or sign in with `gcloud auth login`.");
}
AccessToken GoogleTokenSource::token() {
  std::lock_guard lock(mutex);
  if (stash && (!stash->expiration || options.now()+options.refreshMargin < *stash->expiration)) return *stash;
  stash = fetch();
  return *stash;
}
void GoogleTokenSource::invalidate() {
  std::lock_guard lock(mutex);
  stash.reset();
}
std::string GoogleTokenSource::credentialsProject() const {
  auto path = credentialsFilePath(options);
  if (path.empty()) {
    const auto known = wellKnownCredentialsPath();
    if (known.empty() || !std::filesystem::exists(known)) return {};
    path = known.string();
  }
  try {
    const auto json = readJson(path);
    if (!json) return {};
    auto project = json->value("project_id","");
    return project.empty() ? json->value("quota_project_id","") : project;
  } catch (const std::exception&) { return {}; }
}
} // namespace batbox::providers::gcp
