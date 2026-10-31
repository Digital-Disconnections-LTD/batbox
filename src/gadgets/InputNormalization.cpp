#include "gadgets/InputNormalization.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace batbox::gadgets {
namespace {
void required(bool bound, const char* name) {
  if (!bound) throw std::invalid_argument(std::string("Missing tool input binding: ") + name);
}
Json field(const Json& object, const char* key) {
  return object.is_object() && object.contains(key) ? object.at(key) : Json();
}
bool nullish(const Json& value) { return value.is_null(); }
Json firstNonNullish(const Json& a, const Json& b) { return nullish(a) ? b : a; }
std::string stringField(const Json& object, const char* key) { return object.at(key).get<std::string>(); }
std::string replaceFirst(std::string text, const std::string& needle, const std::string& replacement) {
  const auto pos = text.find(needle);
  if (pos != std::string::npos) text.replace(pos, needle.size(), replacement);
  return text;
}
std::string collapseDoubleBackslashSemicolon(const std::string& text) {
  std::string out;
  for (std::size_t i = 0; i < text.size();) {
    if (i + 2 < text.size() && text[i] == '\\' && text[i + 1] == '\\' && text[i + 2] == ';') {
      out += "\\;"; i += 3;
    } else out += text[i++];
  }
  return out;
}
bool markdownPath(const std::string& path) {
  std::string lower = path;
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return (lower.size() >= 3 && lower.substr(lower.size() - 3) == ".md") ||
    (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".mdx");
}
char32_t codepoint(const std::string& text, std::size_t index) {
  const auto lead = static_cast<unsigned char>(text[index]);
  if (lead < 0x80) return lead;
  if (lead < 0xe0) return ((lead & 0x1f) << 6) | (static_cast<unsigned char>(text[index + 1]) & 0x3f);
  if (lead < 0xf0) return ((lead & 0x0f) << 12) |
    ((static_cast<unsigned char>(text[index + 1]) & 0x3f) << 6) |
    (static_cast<unsigned char>(text[index + 2]) & 0x3f);
  return ((lead & 0x07) << 18) | ((static_cast<unsigned char>(text[index + 1]) & 0x3f) << 12) |
    ((static_cast<unsigned char>(text[index + 2]) & 0x3f) << 6) |
    (static_cast<unsigned char>(text[index + 3]) & 0x3f);
}
bool ecmaWhitespace(char32_t cp) {
  return cp == 0x09 || cp == 0x0a || cp == 0x0b || cp == 0x0c || cp == 0x0d ||
    cp == 0x20 || cp == 0xa0 || cp == 0x1680 || (cp >= 0x2000 && cp <= 0x200a) ||
    cp == 0x2028 || cp == 0x2029 || cp == 0x202f || cp == 0x205f || cp == 0x3000 || cp == 0xfeff;
}
std::string trimEndEcma(const std::string& text) {
  std::size_t end = text.size();
  while (end > 0) {
    std::size_t start = end - 1;
    while (start > 0 && (static_cast<unsigned char>(text[start]) & 0xc0) == 0x80) --start;
    if (!ecmaWhitespace(codepoint(text, start))) break;
    end = start;
  }
  return text.substr(0, end);
}
} // namespace

std::string stripTrailingWhitespace(const std::string& text) {
  std::string result;
  std::size_t start = 0;
  while (start < text.size()) {
    std::size_t end = start;
    while (end < text.size() && text[end] != '\r' && text[end] != '\n') ++end;
    result += trimEndEcma(text.substr(start, end - start));
    if (end == text.size()) return result;
    if (text[end] == '\r' && end + 1 < text.size() && text[end + 1] == '\n') {
      result += "\r\n"; start = end + 2;
    } else { result += text[end]; start = end + 1; }
  }
  return result;
}

Json normalizeGadgetInput(const Gadget& gadget, const Json& input,
                        const std::optional<std::string>& batId,
                        const GadgetInputServices& services) {
  if (gadget.name == services.exitPlanModeGadgetName) {
    required(static_cast<bool>(services.getPlan), "getPlan");
    required(static_cast<bool>(services.getPlanFilePath), "getPlanFilePath");
    required(static_cast<bool>(services.persistFileSnapshotIfRemote), "persistFileSnapshotIfRemote");
    const auto plan = services.getPlan(batId);
    const std::string path = services.getPlanFilePath(batId);
    services.persistFileSnapshotIfRemote();
    if (!plan) return input;
    Json out = input; out["plan"] = *plan; out["planFilePath"] = path; return out;
  }
  if (gadget.name == services.bashGadgetName) {
    required(static_cast<bool>(services.parseBashInput), "parseBashInput");
    const Json parsed = services.parseBashInput(input);
    required(static_cast<bool>(services.getCwd), "getCwd");
    const std::string cwd = services.getCwd();
    std::string command = replaceFirst(stringField(parsed, "command"), "cd " + cwd + " && ", "");
    required(static_cast<bool>(services.getPlatform), "getPlatform");
    if (services.getPlatform() == "windows") {
      required(static_cast<bool>(services.windowsPathToPosixPath), "windowsPathToPosixPath");
      command = replaceFirst(command, "cd " + services.windowsPathToPosixPath(cwd) + " && ", "");
    }
    command = collapseDoubleBackslashSemicolon(command);
    Json out = {{"command", command}};
    for (const char* key : {"description", "timeout", "run_in_background", "dangerouslyDisableSandbox"})
      if (parsed.contains(key)) out[key] = parsed.at(key);
    return out;
  }
  if (gadget.name == services.fileEditGadgetName) {
    required(static_cast<bool>(services.parseFileEditInput), "parseFileEditInput");
    required(static_cast<bool>(services.normalizeFileEditInput), "normalizeFileEditInput");
    const Json parsed = services.parseFileEditInput(input);
    Json edit = Json::object();
    for (const char* key : {"old_string", "new_string", "replace_all"})
      if (parsed.contains(key)) edit[key] = parsed.at(key);
    const Json normalized = services.normalizeFileEditInput(Json{{"file_path", parsed.at("file_path")},
      {"edits", Json::array({edit})}});
    const Json& newEdit = normalized.at("edits").at(0);
    Json out = Json::object();
    if (newEdit.contains("replace_all")) out["replace_all"] = newEdit.at("replace_all");
    out["file_path"] = normalized.at("file_path");
    if (newEdit.contains("old_string")) out["old_string"] = newEdit.at("old_string");
    if (newEdit.contains("new_string")) out["new_string"] = newEdit.at("new_string");
    return out;
  }
  if (gadget.name == services.fileWriteGadgetName) {
    required(static_cast<bool>(services.parseFileWriteInput), "parseFileWriteInput");
    const Json parsed = services.parseFileWriteInput(input);
    const std::string path = stringField(parsed, "file_path");
    const std::string content = stringField(parsed, "content");
    return Json{{"file_path", path}, {"content", markdownPath(path) ? content : stripTrailingWhitespace(content)}};
  }
  if (gadget.name == services.missionOutputGadgetName) {
    const Json missionId = firstNonNullish(field(input, "task_id"),
      firstNonNullish(field(input, "agentId"), field(input, "bash_id")));
    const Json wait = field(input, "wait_up_to");
    const Json legacyTimeout = wait.is_number() ? Json(wait.get<double>() * 1000) : Json();
    const Json timeout = firstNonNullish(field(input, "timeout"), legacyTimeout);
    return Json{{"task_id", nullish(missionId) ? Json("") : missionId},
      {"block", firstNonNullish(field(input, "block"), Json(true))},
      {"timeout", nullish(timeout) ? Json(30000) : timeout}};
  }
  return input;
}

Json normalizeGadgetInputForAPI(const Gadget& gadget, const Json& input,
                              const GadgetInputServices& services) {
  if (gadget.name == services.exitPlanModeGadgetName && input.is_object() &&
      (input.contains("plan") || input.contains("planFilePath"))) {
    Json out = input; out.erase("plan"); out.erase("planFilePath"); return out;
  }
  if (gadget.name == services.fileEditGadgetName && input.is_object() && input.contains("edits")) {
    Json out = input;
    out.erase("old_string"); out.erase("new_string"); out.erase("replace_all");
    return out;
  }
  return input;
}

} // namespace batbox::gadgets
