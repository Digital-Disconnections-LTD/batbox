#include "interactive/GadgetClassification.hpp"
#include <stdexcept>

namespace batbox::interactive {
namespace {
const Json* member(const Json& object, const char* key) {
  if (!object.is_object()) return nullptr;
  const auto found = object.find(key);
  return found == object.end() ? nullptr : &*found;
}
std::string value(const Json& object, const char* key) {
  const auto* field = member(object, key);
  return field && field->is_string() ? field->get<std::string>() : "";
}
template<class T> void need(const T& callback, const char* name) {
  if (!callback) throw std::logic_error(std::string(name) + " import required");
}
bool hasFeature(const GadgetClassificationImports& imports, const std::string& name) {
  need(imports.feature, "feature");
  return imports.feature(name);
}
bool fullscreen(const GadgetClassificationImports& imports) {
  need(imports.isFullscreen, "isFullscreen");
  return imports.isFullscreen();
}
} // namespace

std::optional<std::string> getFilePathFromGadgetInput(const Json& input) {
  const auto* filePath = member(input, "file_path");
  if (filePath && !filePath->is_null())
    return filePath->is_string() ? filePath->get<std::string>() : filePath->dump();
  const auto* path = member(input, "path");
  if (path && !path->is_null())
    return path->is_string() ? path->get<std::string>() : path->dump();
  return {};
}

bool isMemorySearch(const Json& input, const GadgetClassificationImports& imports) {
  if (input.is_null()) return false;
  if (const auto path = value(input, "path"); !path.empty()) {
    need(imports.isAutoManagedMemoryFile, "isAutoManagedMemoryFile");
    need(imports.isMemoryDirectory, "isMemoryDirectory");
    if (imports.isAutoManagedMemoryFile(path) || imports.isMemoryDirectory(path)) return true;
  }
  if (const auto glob = value(input, "glob"); !glob.empty()) {
    need(imports.isAutoManagedMemoryPattern, "isAutoManagedMemoryPattern");
    if (imports.isAutoManagedMemoryPattern(glob)) return true;
  }
  if (const auto command = value(input, "command"); !command.empty()) {
    need(imports.isShellCommandTargetingMemory, "isShellCommandTargetingMemory");
    if (imports.isShellCommandTargetingMemory(command)) return true;
  }
  return false;
}

bool isMemoryWriteOrEdit(const std::string& gadgetName, const Json& input,
                         const GadgetClassificationImports& imports) {
  if (gadgetName != imports.writeGadgetName && gadgetName != imports.editGadgetName) return false;
  const auto path = getFilePathFromGadgetInput(input);
  if (!path) return false;
  need(imports.isAutoManagedMemoryFile, "isAutoManagedMemoryFile");
  return imports.isAutoManagedMemoryFile(*path);
}

SearchOrReadResult getGadgetSearchOrReadInfo(const std::string& gadgetName,
                                           const Json& input,
                                           const gadgets::Gadgets& available,
                                           const GadgetClassificationImports& imports) {
  if (gadgetName == imports.replGadgetName)
    return {.isCollapsible=true, .isREPL=true, .isAbsorbedSilently=true};
  if (isMemoryWriteOrEdit(gadgetName, input, imports))
    return {.isCollapsible=true, .isMemoryWrite=true};
  if ((hasFeature(imports, "HISTORY_SNIP") && imports.snipGadgetName &&
       gadgetName == *imports.snipGadgetName) ||
      (fullscreen(imports) && gadgetName == imports.gadgetSearchName))
    return {.isCollapsible=true, .isAbsorbedSilently=true};

  const auto* gadget = gadgets::findGadgetByName(available, gadgetName);
  if (!gadget) {
    need(imports.getReplPrimitiveGadgets, "getReplPrimitiveTools");
    const auto primitive = imports.getReplPrimitiveGadgets();
    gadget = gadgets::findGadgetByName(primitive, gadgetName);
  }
  if (!gadget || !gadget->isSearchOrReadCommand) return {};
  const Json result = gadget->isSearchOrReadCommand(input);
  const bool isSearch = value::truthy(value::property(result, "isSearch"));
  const bool isRead = value::truthy(value::property(result, "isRead"));
  const bool isList = value::truthy(value::property(result, "isList"));
  const bool collapsible = isSearch || isRead || isList;
  SearchOrReadResult out;
  out.isCollapsible = collapsible || (fullscreen(imports) ?
      gadgetName == imports.bashGadgetName : false);
  out.isSearch = isSearch;
  out.isRead = isRead;
  out.isList = isList;
  if (gadget->isMcp.value_or(false)) {
    const auto* serverName = member(gadget->mcpInfo, "serverName");
    if (serverName && serverName->is_string())
      out.mcpServerName = serverName->get<std::string>();
  }
  out.isBash = fullscreen(imports) ?
      std::optional<bool>{!collapsible && gadgetName == imports.bashGadgetName} :
      std::nullopt;
  return out;
}

std::optional<SearchOrReadResult> getSearchOrReadFromContent(
    const Json* content, const gadgets::Gadgets& available,
    const GadgetClassificationImports& imports) {
  if (!content || value(*content, "type") != "tool_use") return {};
  const auto name = value(*content, "name");
  if (name.empty()) return {};
  const auto* input = member(*content, "input");
  const auto result = getGadgetSearchOrReadInfo(name,
      input ? *input : Json(nullptr), available, imports);
  if (!result.isCollapsible && !result.isREPL) return {};
  return result;
}
} // namespace batbox::interactive
