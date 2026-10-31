#pragma once

#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::interactive {

struct SearchOrReadResult {
  bool isCollapsible = false;
  bool isSearch = false;
  bool isRead = false;
  bool isList = false;
  bool isREPL = false;
  bool isMemoryWrite = false;
  bool isAbsorbedSilently = false;
  std::optional<std::string> mcpServerName;
  std::optional<bool> isBash;
};

struct GadgetClassificationImports {
  std::function<bool(const std::string&)> feature;
  std::function<bool()> isFullscreen;
  std::function<bool(const std::string&)> isAutoManagedMemoryFile;
  std::function<bool(const std::string&)> isMemoryDirectory;
  std::function<bool(const std::string&)> isAutoManagedMemoryPattern;
  std::function<bool(const std::string&)> isShellCommandTargetingMemory;
  std::function<gadgets::Gadgets()> getReplPrimitiveGadgets;
  std::string replGadgetName;
  std::string writeGadgetName;
  std::string editGadgetName;
  std::string gadgetSearchName;
  std::optional<std::string> snipGadgetName;
  std::string bashGadgetName;
};

std::optional<std::string> getFilePathFromGadgetInput(const Json& input);
bool isMemorySearch(const Json& input, const GadgetClassificationImports& imports);
bool isMemoryWriteOrEdit(const std::string& gadgetName, const Json& input,
                         const GadgetClassificationImports& imports);
SearchOrReadResult getGadgetSearchOrReadInfo(const std::string& gadgetName,
                                           const Json& input,
                                           const gadgets::Gadgets& gadgets,
                                           const GadgetClassificationImports& imports);
std::optional<SearchOrReadResult> getSearchOrReadFromContent(
    const Json* content, const gadgets::Gadgets& gadgets,
    const GadgetClassificationImports& imports);

} // namespace batbox::interactive
