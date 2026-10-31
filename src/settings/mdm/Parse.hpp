#pragma once
#include "settings/Parse.hpp"
#include "settings/Paths.hpp"
#include "settings/mdm/RawRead.hpp"
#include "settings/mdm/Result.hpp"
#include "settings/persistence/SafeJson.hpp"
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace batbox::settings::mdm {
struct ParsedRaw {MdmResult mdm,hkcu;};
struct DirEntry {std::string name;bool file=false,symlink=false;};
struct ParseServices {
  persistence::SafeJsonParser* safeJson=nullptr;
  settings::ParseServices parser;
  Paths* paths=nullptr;
  Names names;
  std::function<std::string(const std::string&)> readFileSync;
  std::function<std::vector<DirEntry>(const std::string&)> readdirSync;
};
MdmResult parseCommandOutputAsSettings(const std::string& stdoutText,
  const std::string& sourcePath,const ParseServices& services);
std::optional<std::string> parseRegQueryStdout(const std::string& stdoutText,
  const std::string& valueName="Settings");
bool hasManagedSettingsFile(const ParseServices& services);
ParsedRaw consumeRawReadResult(const RawReadResult& raw,
  const ParseServices& services);
ParseServices indigenousParseServices(Paths& paths,
  persistence::SafeJsonParser& safeJson,settings::ParseServices parser,
  Names names={});
}
