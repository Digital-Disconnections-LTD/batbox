#include "gadgets/files/FileWrite.hpp"
#include <cmath>
#include <stdexcept>
#include <system_error>

namespace batbox::gadgets::files {
namespace {
void require(bool bound, const char* name) {
  if (!bound) throw std::invalid_argument(std::string("Missing FileWrite binding: ") + name);
}
} // namespace

ValidationResult validateWriteInput(const Json& input, const FileWriteServices& services) {
  const std::string fullPath = filesystem::expandPath(input.at("file_path").get<std::string>(), services.paths);
  const std::string content = input.at("content").get<std::string>();
  require(static_cast<bool>(services.checkTeamMemSecrets), "checkTeamMemSecrets");
  if (const auto error = services.checkTeamMemSecrets(fullPath, content))
    return {false, *error, 0};
  require(static_cast<bool>(services.matchingEditDenyRule), "matchingEditDenyRule");
  if (services.matchingEditDenyRule(fullPath))
    return {false, "File is in a directory that is denied by your permission settings.", 1};
  if (fullPath.rfind("\\\\", 0) == 0 || fullPath.rfind("//", 0) == 0)
    return {true, std::nullopt, std::nullopt};
  double mtime;
  try { mtime = filesystem::getFileModificationTimeRaw(fullPath); }
  catch (const std::system_error& error) {
    if (error.code() == std::errc::no_such_file_or_directory)
      return {true, std::nullopt, std::nullopt};
    throw;
  }
  require(services.readFileState != nullptr, "readFileState");
  const auto found = services.readFileState->get(fullPath);
  if (!found || found->isPartialView)
    return {false, "File has not been read yet. Read it first before writing to it.", 2};
  if (std::floor(mtime) > found->timestamp)
    return {false, "File has been modified since read, either by the user or by a linter. Read it again before attempting to write it.", 3};
  return {true, std::nullopt, std::nullopt};
}

} // namespace batbox::gadgets::files
