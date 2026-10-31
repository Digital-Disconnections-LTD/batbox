#pragma once
#include "gadgets/files/EditUtils.hpp"
#include "gadgets/files/FileWrite.hpp"

namespace batbox::gadgets::files {
struct FileEditServices : FileWriteServices {
  edit::Services editUtils;
  std::function<bool()> simpleMode;
  std::function<std::string(const std::string&)> findSimilarFile;
  std::function<std::optional<std::string>(const std::string&)> suggestPathUnderCwd;
  std::function<std::optional<ValidationResult>(const std::string&,const std::string&,
    const std::function<std::string()>&)> validateInputForSettingsFileEdit;
  std::function<std::string(double)> formatFileSize;
  std::function<bool()> compactLinePrefixEnabled;
  std::function<bool()> conciseEditHint;
  std::string fileNotFoundCwdNote="Note: your current working directory is";
  std::string unexpectedModifiedError="File has been unexpectedly modified. Read it again before attempting to write it.";
  std::string notebookEditGadgetName="NotebookEdit";
};
ValidationResult validateEditInput(const Json& input,const FileEditServices& services);
GadgetResult callEdit(const Json& input,const std::string& parentMessageUUID,
                    const GadgetCallContext& context,FileEditServices& services);
Json mapEditResult(const Json& data,const std::string& gadgetUseID);
} // namespace batbox::gadgets::files
