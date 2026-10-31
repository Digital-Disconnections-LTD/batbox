#include "gadgets/files/FileEdit.hpp"

namespace batbox::gadgets::files {
Json mapEditResult(const Json& data,const std::string& gadgetUseID){
  const auto path=data.at("filePath").get<std::string>();
  const bool userModified=data.at("userModified").get<bool>();
  const bool replaceAll=data.at("replaceAll").get<bool>();
  const std::string note=userModified
    ? ".  The user modified your proposed changes before accepting them. " : "";
  const std::string content=replaceAll
    ? "The file "+path+" has been updated"+note+". All occurrences were successfully replaced."
    : "The file "+path+" has been updated successfully"+note+".";
  return Json{{"tool_use_id",gadgetUseID},{"type","tool_result"},{"content",content}};
}
} // namespace batbox::gadgets::files
