#pragma once
#include <string>
#include <vector>

namespace batbox::settings {
enum class Source { User, Project, Local, Flag, Policy };
std::string sourceKey(Source source);
Source sourceFromKey(const std::string& key);
std::vector<Source> settingSources();
std::string getSettingSourceName(Source source);
std::string getSourceDisplayName(const std::string& source);
std::string getSettingSourceDisplayNameLowercase(const std::string& source);
std::string getSettingSourceDisplayNameCapitalized(const std::string& source);
std::vector<Source> parseSettingSourcesFlag(const std::string& flag);
std::vector<Source> getEnabledSettingSources(
  const std::vector<Source>& allowed);
bool isSettingSourceEnabled(Source source,
  const std::vector<Source>& allowed);
std::vector<Source> editableSources();
}
