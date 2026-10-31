#pragma once
#include "filesystem/FileOps.hpp"
#include "runtime/JsonValue.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::gadgets::files::edit {
struct Services {
  filesystem::PathServices paths;
  std::function<Json(const std::string&,const std::string&,const std::string&)> getPatchForDisplay;
  std::function<Json(const std::string&,const std::string&,const std::string&)> getPatchFromContents;
  std::function<std::string(const std::string&)> readFileSyncStashed;
};
std::string normalizeQuotes(std::string text);
std::string stripTrailingWhitespace(const std::string& text);
std::optional<std::string> findActualString(const std::string& fileContent,const std::string& searchString);
std::string preserveQuoteStyle(const std::string& oldString,const std::string& actualOldString,
                               const std::string& newString);
std::string applyEditToFile(const std::string& original,const std::string& oldString,
                            const std::string& newString,bool replaceAll=false);
std::string convertLeadingTabsToSpaces(const std::string& content);
Json getPatchForEdits(const std::string& filePath,const std::string& fileContents,
                      const Json& edits,const Services& services);
Json getPatchForEdit(const std::string& filePath,const std::string& fileContents,
                     const std::string& oldString,const std::string& newString,
                     bool replaceAll,const Services& services);
Json normalizeFileEditInput(const Json& input,const Services& services);
bool areFileEditsEquivalent(const Json& edits1,const Json& edits2,
                            const std::string& originalContent,const Services& services);
bool areFileEditsInputsEquivalent(const Json& input1,const Json& input2,const Services& services);
} // namespace batbox::gadgets::files::edit
