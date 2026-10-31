#include "gadgets/files/FileWrite.hpp"
#include "gadgets/files/ActiveTriggers.hpp"
#include <filesystem>
#include <stdexcept>
#include <system_error>

namespace batbox::gadgets::files {
namespace {
void require(bool bound, const char* name) {
  if (!bound) throw std::invalid_argument(std::string("Missing FileWrite binding: ") + name);
}
} // namespace

GadgetResult callWrite(const Json& input, const std::string& parentMessageUUID,
                     const GadgetCallContext& context, FileWriteServices& services) {
  (void)context;
  const std::string requestedPath = input.at("file_path").get<std::string>();
  const std::string content = input.at("content").get<std::string>();
  const std::string fullPath = filesystem::expandPath(requestedPath, services.paths);
  const std::string directory = std::filesystem::path(fullPath).parent_path().string();
  require(static_cast<bool>(services.paths.getCwd), "getCwd");
  const std::string cwd = services.paths.getCwd();
  require(static_cast<bool>(services.discoverSkillDirsForPaths), "discoverSkillDirsForPaths");
  const std::vector<std::string> newSkillDirs = services.discoverSkillDirsForPaths(fullPath, cwd);
  if (!newSkillDirs.empty()) {
    for(const auto& dir:newSkillDirs)recordDynamicSkillDir(services,dir);
    require(static_cast<bool>(services.addSkillDirectories), "addSkillDirectories");
    services.addSkillDirectories(newSkillDirs);
  }
  require(static_cast<bool>(services.activateConditionalSkillsForPaths), "activateConditionalSkillsForPaths");
  services.activateConditionalSkillsForPaths(fullPath, cwd);
  require(static_cast<bool>(services.diagnosticBeforeFileEdited), "diagnosticBeforeFileEdited");
  services.diagnosticBeforeFileEdited(fullPath);
  std::filesystem::create_directories(directory);
  require(static_cast<bool>(services.fileHistoryEnabled), "fileHistoryEnabled");
  if (services.fileHistoryEnabled()) {
    require(static_cast<bool>(services.fileHistoryTrackEdit), "fileHistoryTrackEdit");
    services.fileHistoryTrackEdit(fullPath, parentMessageUUID);
  }
  std::optional<filesystem::FileMetadata> meta;
  try { meta = filesystem::readFileSyncWithMetadata(fullPath); }
  catch (const std::system_error& error) {
    if (error.code() != std::errc::no_such_file_or_directory) throw;
  }
  require(services.readFileState != nullptr, "readFileState");
  if (meta) {
    const double lastWriteTime = filesystem::getFileModificationTime(fullPath);
    const auto found = services.readFileState->get(fullPath);
    if (!found || lastWriteTime > found->timestamp) {
      const bool fullRead = found && !found->offset && !found->limit;
      if (!fullRead || meta->content != found->content)
        throw std::runtime_error("File has been unexpectedly modified. Read it again before attempting to write it.");
    }
  }
  const std::string encoding = meta ? meta->encoding : "utf8";
  const std::optional<std::string> oldContent = meta ? std::optional<std::string>(meta->content) : std::nullopt;
  filesystem::writeTextContent(fullPath, content, encoding, "LF");
  require(static_cast<bool>(services.lspAvailable), "lspAvailable");
  if (services.lspAvailable()) {
    require(static_cast<bool>(services.clearDeliveredDiagnostics), "clearDeliveredDiagnostics");
    require(static_cast<bool>(services.lspChangeFile), "lspChangeFile");
    require(static_cast<bool>(services.lspSaveFile), "lspSaveFile");
    services.clearDeliveredDiagnostics("file://" + fullPath);
    services.lspChangeFile(fullPath, content);
    services.lspSaveFile(fullPath);
  }
  require(static_cast<bool>(services.notifyEditorFileUpdated), "notifyEditorFileUpdated");
  services.notifyEditorFileUpdated(fullPath, oldContent, content);
  services.readFileState->set(fullPath,ReadSnapshot{content, filesystem::getFileModificationTime(fullPath), false,
    std::nullopt, std::nullopt});
  Json gitDiff;
  require(static_cast<bool>(services.remoteDiffEnabled), "remoteDiffEnabled");
  if (services.remoteDiffEnabled()) {
    require(static_cast<bool>(services.fetchSingleFileGitDiff), "fetchSingleFileGitDiff");
    if (const auto diff = services.fetchSingleFileGitDiff(fullPath)) gitDiff = *diff;
  }
  Json data;
  if (oldContent && !oldContent->empty()) {
    require(static_cast<bool>(services.getPatchForDisplay), "getPatchForDisplay");
    data = {{"type", "update"}, {"filePath", requestedPath}, {"content", content},
      {"structuredPatch", services.getPatchForDisplay(requestedPath, *oldContent, content)},
      {"originalFile", *oldContent}};
  } else {
    data = {{"type", "create"}, {"filePath", requestedPath}, {"content", content},
      {"structuredPatch", Json::array()}, {"originalFile", nullptr}};
  }
  if (!gitDiff.is_null()) data["gitDiff"] = gitDiff;
  GadgetResult result;
  result.data = std::move(data);
  return result;
}

} // namespace batbox::gadgets::files
