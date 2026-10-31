#include "gadgets/files/FileWrite.hpp"

namespace batbox::gadgets::files {

Json mapWriteResult(const Json& data, const std::string& gadgetUseID) {
  const std::string path = data.at("filePath").get<std::string>();
  const std::string type = data.at("type").get<std::string>();
  if (type == "create")
    return Json{{"tool_use_id", gadgetUseID}, {"type", "tool_result"},
      {"content", "File created successfully at: " + path}};
  if (type == "update")
    return Json{{"tool_use_id", gadgetUseID}, {"type", "tool_result"},
      {"content", "The file " + path + " has been updated successfully."}};
  return Json();
}

} // namespace batbox::gadgets::files
