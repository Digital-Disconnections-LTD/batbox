#include "cloud_check.hpp"
#include "providers/aws/Credentials.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

using namespace batbox::providers;
using cloud_check::check;
using cloud_check::same;
using cloud_check::throwsContaining;
namespace fs = std::filesystem;

namespace {
fs::path home;
void write(const fs::path& path, const std::string& text) {
  fs::create_directories(path.parent_path());
  std::ofstream(path,std::ios::binary) << text;
}
std::string read(const fs::path& path) {
  std::ifstream input(path);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  return buffer.str();
}
void clearAwsEnvironment() {
  for (const auto* name : {"AWS_ACCESS_KEY_ID","AWS_SECRET_ACCESS_KEY","AWS_SESSION_TOKEN","AWS_SECURITY_TOKEN","AWS_PROFILE",
                           "AWS_DEFAULT_PROFILE","AWS_REGION","AWS_DEFAULT_REGION","AWS_BEARER_TOKEN_BEDROCK","AWS_CONFIG_FILE",
                           "AWS_SHARED_CREDENTIALS_FILE","AWS_ENDPOINT_URL","AWS_ENDPOINT_URL_STS"})
    unsetenv(name);
}
void reset() {
  clearAwsEnvironment();
  fs::remove_all(home/".aws");
}
struct FakeClock {
  std::chrono::system_clock::time_point now = std::chrono::system_clock::time_point(std::chrono::seconds(1793000000));
};
aws::CredentialOptions clocked(FakeClock& clock) {
  aws::CredentialOptions options;
  options.now = [&clock] { return clock.now; };
  return options;
}
std::string processScript(const std::string& name, const std::string& json) {
  const auto script = home/(name+".sh");
  const auto counter = home/(name+".count");
  write(script,"#!/bin/sh\necho x >> '"+counter.string()+"'\ncat <<'JSON'\n"+json+"\nJSON\n");
  fs::permissions(script,fs::perms::owner_all);
  return script.string();
}
std::size_t runs(const std::string& name) {
  const auto text = read(home/(name+".count"));
  return static_cast<std::size_t>(std::count(text.begin(),text.end(),'\n'));
}
} // namespace

int main() {
  char pattern[] = "/tmp/batbox-cloud-credentials-XXXXXX";
  if (!mkdtemp(pattern)) return 2;
  home = pattern;
  setenv("HOME",home.c_str(),1);
  reset();

  {
    const auto ini = aws::parseIni("# comment\r\n[default]\r\nregion = us-west-2\r\n; another\n[profile dev]\naws_access_key_id=AKIADEV\n"
                                   "s3 =\n  max_concurrent_requests = 20\nRegion = eu-west-1\n");
    same(ini.at("default").at("region"),"us-west-2","INI with CRLF");
    same(ini.at("profile dev").at("aws_access_key_id"),"AKIADEV","profile section");
    same(ini.at("profile dev").at("region"),"eu-west-1","keys are case-insensitive");
    check(!ini.at("profile dev").contains("max_concurrent_requests"),"nested settings are skipped");
  }
  {
    reset();
    setenv("AWS_ACCESS_KEY_ID","AKIAENV",1);
    setenv("AWS_SECRET_ACCESS_KEY","envsecret",1);
    setenv("AWS_SESSION_TOKEN","envtoken",1);
    write(home/".aws"/"credentials","[default]\naws_access_key_id = AKIAFILE\naws_secret_access_key = filesecret\n");
    aws::CredentialChain chain;
    const auto auth = chain.resolve();
    check(auth.kind == aws::BedrockAuth::Kind::SigV4,"environment keys sign requests");
    same(auth.credentials.accessKeyId,"AKIAENV","environment beats the credentials file");
    same(auth.credentials.sessionToken,"envtoken","session token from the environment");
    same(auth.source,"environment","source label");
  }
  {
    reset();
    setenv("AWS_ACCESS_KEY_ID","AKIAENV",1);
    setenv("AWS_SECRET_ACCESS_KEY","envsecret",1);
    setenv("AWS_BEARER_TOKEN_BEDROCK","bedrock-api-key",1);
    aws::CredentialChain chain;
    auto auth = chain.resolve();
    check(auth.kind == aws::BedrockAuth::Kind::Bearer,"Bedrock API key wins over keys");
    same(auth.bearerToken,"bedrock-api-key","bearer from AWS_BEARER_TOKEN_BEDROCK");
    aws::CredentialOptions options;
    options.bearerToken = "endpoint-key";
    aws::CredentialChain configured(options);
    same(configured.resolve().bearerToken,"endpoint-key","endpoint auth_token wins over the environment");
  }
  {
    reset();
    write(home/".aws"/"credentials","[default]\naws_access_key_id = AKIADEFAULT\naws_secret_access_key = s\n"
                                    "[dev]\naws_access_key_id = AKIADEV\naws_secret_access_key = devsecret\naws_session_token = devtoken\n");
    write(home/".aws"/"config","[default]\nregion = us-east-2\n[profile dev]\nregion = eu-central-1\n");
    aws::CredentialChain fallback;
    same(fallback.resolve().credentials.accessKeyId,"AKIADEFAULT","default profile");
    same(fallback.region(),"us-east-2","default profile region");
    setenv("AWS_PROFILE","dev",1);
    aws::CredentialChain chain;
    const auto auth = chain.resolve();
    same(auth.credentials.accessKeyId,"AKIADEV","AWS_PROFILE selects the profile");
    same(auth.credentials.sessionToken,"devtoken","profile session token");
    same(auth.source,"profile dev","profile source label");
    same(chain.region(),"eu-central-1","profile region");
    setenv("AWS_DEFAULT_REGION","ap-southeast-2",1);
    same(chain.region(),"ap-southeast-2","AWS_DEFAULT_REGION beats the profile");
    setenv("AWS_REGION","ca-central-1",1);
    same(chain.region(),"ca-central-1","AWS_REGION beats AWS_DEFAULT_REGION");
  }
  {
    reset();
    const auto other = home/"elsewhere";
    write(other/"creds","[ops]\naws_access_key_id = AKIAOPS\naws_secret_access_key = ops\n");
    write(other/"conf","[profile ops]\nregion = sa-east-1\n");
    setenv("AWS_SHARED_CREDENTIALS_FILE",(other/"creds").c_str(),1);
    setenv("AWS_CONFIG_FILE",(other/"conf").c_str(),1);
    aws::CredentialOptions options;
    options.profile = "ops";
    aws::CredentialChain chain(options);
    same(chain.resolve().credentials.accessKeyId,"AKIAOPS","AWS_SHARED_CREDENTIALS_FILE");
    same(chain.region(),"sa-east-1","AWS_CONFIG_FILE");
  }
  {
    reset();
    FakeClock clock;
    const auto command = processScript("expiring","{\"Version\":1,\"AccessKeyId\":\"AKIAPROC\",\"SecretAccessKey\":\"procsecret\","
                                                  "\"SessionToken\":\"proctoken\",\"Expiration\":\"2026-10-26T08:53:20Z\"}");
    // 1793000000 is 2026-10-26T07:33:20Z, so the credentials expire 80 minutes after the fake clock starts.
    write(home/".aws"/"config","[profile proc]\ncredential_process = "+command+"\n");
    setenv("AWS_PROFILE","proc",1);
    aws::CredentialChain chain(clocked(clock));
    auto auth = chain.resolve();
    same(auth.credentials.accessKeyId,"AKIAPROC","credential_process keys");
    same(auth.credentials.sessionToken,"proctoken","credential_process token");
    check(auth.credentials.expiration.has_value(),"credential_process expiration parsed");
    chain.resolve();
    clock.now += std::chrono::minutes(70);
    chain.resolve();
    check(runs("expiring") == 1,"credential_process output is stashed until near expiry, ran "+std::to_string(runs("expiring")));
    clock.now += std::chrono::minutes(6);
    chain.resolve();
    check(runs("expiring") == 2,"credential_process reruns within the refresh margin, ran "+std::to_string(runs("expiring")));
    chain.invalidate();
    chain.resolve();
    check(runs("expiring") == 3,"invalidate forces a rerun");
  }
  {
    reset();
    FakeClock clock;
    const auto command = processScript("permanent","{\"Version\":1,\"AccessKeyId\":\"AKIAPERM\",\"SecretAccessKey\":\"p\"}");
    write(home/".aws"/"config","[default]\ncredential_process = "+command+"\n");
    aws::CredentialChain chain(clocked(clock));
    chain.resolve();
    clock.now += std::chrono::hours(24*30);
    chain.resolve();
    check(runs("permanent") == 1,"credential_process without Expiration is reused");
  }
  {
    reset();
    const auto bad = processScript("bad","{\"Version\":2,\"AccessKeyId\":\"A\",\"SecretAccessKey\":\"B\"}");
    write(home/".aws"/"config","[default]\ncredential_process = "+bad+"\n");
    throwsContaining([] { aws::CredentialChain().resolve(); },"Version","credential_process version check");
    write(home/".aws"/"config","[default]\ncredential_process = /bin/sh -c 'exit 3'\n");
    throwsContaining([] { aws::CredentialChain().resolve(); },"status 3","credential_process exit status");
  }
  {
    std::string error;
    check(!aws::parseProcessCredentials("not json",error) && error.find("JSON") != std::string::npos,"process output must be JSON");
    check(!aws::parseProcessCredentials("{\"Version\":1,\"AccessKeyId\":\"A\",\"SecretAccessKey\":\"B\",\"Expiration\":\"soon\"}",error) &&
          error.find("Expiration") != std::string::npos,"process expiration must parse");
  }
  {
    reset();
    write(home/".aws"/"config","[profile a]\nrole_arn = arn:aws:iam::1:role/a\nsource_profile = b\n"
                               "[profile b]\nrole_arn = arn:aws:iam::1:role/b\nsource_profile = a\n");
    setenv("AWS_PROFILE","a",1);
    throwsContaining([] { aws::CredentialChain().resolve(); },"cycle","source_profile cycle");
  }
  {
    reset();
    write(home/".aws"/"config","[profile sso]\nsso_session = corp\nsso_account_id = 1\nsso_role_name = dev\n");
    setenv("AWS_PROFILE","sso",1);
    throwsContaining([] { aws::CredentialChain().resolve(); },"export-credentials","SSO hint");
  }
  {
    reset();
    throwsContaining([] { aws::CredentialChain().resolve(); },"AWS_BEARER_TOKEN_BEDROCK","no credentials at all");
    setenv("AWS_PROFILE","missing",1);
    throwsContaining([] { aws::CredentialChain().resolve(); },"missing was not found","unknown profile");
  }
  reset();
  fs::remove_all(home);
  return cloud_check::finish("cloud-credentials");
}
