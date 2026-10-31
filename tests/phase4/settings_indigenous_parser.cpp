#include "settings/schema/IndigenousBindings.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <unistd.h>
using namespace batbox;
int main(){
  namespace fs=std::filesystem;
  const auto root=fs::temp_directory_path()/("batbox-native-settings-"+std::to_string(::getpid()));
  fs::create_directories(root);const auto path=root/"settings.json";
  std::ofstream(path)<<R"SETTINGS({"model":"fast","env":{"COUNT":12,"FLAG":true},"permissions":{"allow":["Read","Bash()"]},"hooks":{"Stop":[{"hooks":[{"type":"http","url":"https://EXAMPLE.org:443/a b"}]}]},"sandbox":{"network":{"httpProxyPort":1234}}})SETTINGS";
  const auto services=settings::schema::bindIndigenousSettingsParser({}, {},
    "https://example.test/docs");
  settings::Stash stash;
  const auto first=settings::parseSettingsFile(path.string(),stash,services);
  assert(first.settings.is_object());assert(first.settings.at("model")=="fast");
  assert(first.settings.at("env").at("COUNT")=="12");
  assert(first.settings.at("env").at("FLAG")=="true");
  assert(first.settings.at("permissions").at("allow")==Json::array({"Read"}));
  assert(!first.errors.empty());
  assert(first.settings.at("hooks").at("Stop")[0].at("hooks")[0].at("url")
    =="https://example.org/a%20b");
  assert(first.settings.at("sandbox").at("network").at("httpProxyPort")==1234);
  std::ofstream(path)<<R"({"model":"updated"})";
  assert(settings::parseSettingsFile(path.string(),stash,services).settings==first.settings);
  stash.resetSettingsStash();
  assert(settings::parseSettingsFile(path.string(),stash,services).settings.at("model")=="updated");
  const auto missing=settings::parseSettingsFile((root/"missing.json").string(),stash,services);
  assert(missing.settings.is_null()&&missing.errors.empty());
  std::ofstream(root/"invalid.json")<<R"({"bad":)";
  const auto invalid=settings::parseSettingsFile((root/"invalid.json").string(),stash,services);
  assert(invalid.settings.is_null()&&!invalid.errors.empty());
  fs::remove_all(root);
}
