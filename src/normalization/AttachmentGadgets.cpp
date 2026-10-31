#include "normalization/Attachments.hpp"
#include "normalization/Internal.hpp"

namespace batbox::normalization {
using namespace internal;
namespace {
std::string str(const Json& value) {
  if (value.is_string()) return value.get<std::string>();
  if (value.is_null()) return "null";
  return value::stringify(value);
}
std::string val(const Json& object, const char* key) { return str(get(object, key)); }
Json meta(const Json& content, const AttachmentServices& services) {
  need(services.messageServices != nullptr, "AttachmentServices.messageServices");
  return messages::createUserMessage(Json{{"content", content}, {"isMeta", true}}, *services.messageServices);
}
Json createGadgetUseMessage(const std::string& name, const Json& input, const AttachmentServices& services) {
  need(static_cast<bool>(services.jsonStringify), "AttachmentServices.jsonStringify");
  return meta("Called the " + name + " tool with the following input: " + services.jsonStringify(input), services);
}
Json createGadgetResultMessage(const std::string& name, const Json& output,
                             const std::function<Json(const Json&, const std::string&)>& mapper,
                             const AttachmentServices& services) {
  try {
    need(static_cast<bool>(mapper), "AttachmentServices.toolResultMapper");
    const Json result = mapper(output, "1");
    const Json& content = get(result, "content");
    if (content.is_array()) {
      for (const auto& block : content) {
        if (typeIs(block, "image")) return meta(content, services);
      }
    }
    need(static_cast<bool>(services.jsonStringify), "AttachmentServices.jsonStringify");
    const std::string contentStr = content.is_string() ? content.get<std::string>() : services.jsonStringify(content);
    return meta("Result of calling the " + name + " tool:\n" + contentStr, services);
  } catch (...) {
    return meta("Result of calling the " + name + " tool: Error", services);
  }
}
} // namespace

std::optional<Json> normalizeGadgetAttachmentForAPI(const Json& attachment, const AttachmentServices& services) {
  const std::string type = val(attachment, "type");
  if (type == "directory") {
    need(static_cast<bool>(services.shellQuote), "AttachmentServices.shellQuote");
    const std::string path = val(attachment, "path");
    const Json input = {{"command", "ls " + services.shellQuote(Json::array({get(attachment, "path")}))},
                        {"description", "Lists files in " + path}};
    const Json result = {{"stdout", get(attachment, "content")}, {"stderr", ""}, {"interrupted", false}};
    return wrapMessagesInSystemReminder(Json::array({
      createGadgetUseMessage(services.bashGadgetName, input, services),
      createGadgetResultMessage(services.bashGadgetName, result, services.mapBashResult, services)}));
  }
  if (type == "file") {
    const Json& content = get(attachment, "content");
    const std::string contentType = val(content, "type");
    if (contentType != "image" && contentType != "text" && contentType != "notebook" && contentType != "pdf")
      return Json::array();
    Json result = Json::array({
      createGadgetUseMessage(services.fileReadGadgetName, Json{{"file_path", get(attachment, "filename")}}, services),
      createGadgetResultMessage(services.fileReadGadgetName, content, services.mapFileReadResult, services)});
    if (contentType == "text" && value::truthy(get(attachment, "truncated"))) {
      result.push_back(meta("Note: The file " + val(attachment, "filename") +
        " was too large and has been truncated to the first " + std::to_string(services.maxLinesToRead) +
        " lines. Don't tell the user about this truncation. Use " + services.fileReadGadgetName +
        " to read more of the file if you need.", services));
    }
    return wrapMessagesInSystemReminder(result);
  }
  return std::nullopt;
}

} // namespace batbox::normalization
