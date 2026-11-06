#pragma once
#include "filesystem/FileOps.hpp"
#include "file_state_stash/FileStateStash.hpp"
#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace batbox::gadgets::files {

using ReadSnapshot=file_state_stash::FileState;

inline constexpr const char* planModeFileChangeMessage=
  "Plan mode is on, so file changes are blocked. Describe the change you would make "
  "instead; the user can leave plan mode with /permissions.";
struct FileWriteServices {
  filesystem::PathServices paths;
  std::shared_ptr<file_state_stash::FileStateStash> readFileStateOwner;
  file_state_stash::FileStateStash* readFileState = nullptr;
  std::set<std::string>* dynamicSkillDirTriggers = nullptr;
  std::shared_ptr<runtime::OrderedStringSet> activeDynamicSkillDirTriggers;
  std::function<bool(const std::string&)> matchingEditDenyRule;
  // Plan mode blocks every file change; checked ahead of the deny rules so
  // the refusal names the real reason.
  std::function<bool()> planModeActive;
  std::function<std::vector<std::string>(const std::string&, const std::string&)> discoverSkillDirsForPaths;
  std::function<void(const std::vector<std::string>&)> addSkillDirectories;
  std::function<void(const std::string&, const std::string&)> activateConditionalSkillsForPaths;
  std::function<void(const std::string&)> diagnosticBeforeFileEdited;
  std::function<bool()> fileHistoryEnabled;
  std::function<void(const std::string&, const std::string&)> fileHistoryTrackEdit;
  std::function<Json(const std::string&, const std::string&, const std::string&)> getPatchForDisplay;
  std::function<void(const std::string&, const std::optional<std::string>&, const std::string&)> notifyEditorFileUpdated;
  std::function<bool()> lspAvailable;
  std::function<void(const std::string&)> clearDeliveredDiagnostics;
  std::function<void(const std::string&, const std::string&)> lspChangeFile;
  std::function<void(const std::string&)> lspSaveFile;
  std::function<bool()> remoteDiffEnabled;
  std::function<std::optional<Json>(const std::string&)> fetchSingleFileGitDiff;
};

ValidationResult validateWriteInput(const Json& input, const FileWriteServices& services);
GadgetResult callWrite(const Json& input, const std::string& parentMessageUUID,
                     const GadgetCallContext& context, FileWriteServices& services);
Json mapWriteResult(const Json& data, const std::string& gadgetUseID);

} // namespace batbox::gadgets::files
