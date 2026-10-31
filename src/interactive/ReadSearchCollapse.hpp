#pragma once

#include "interactive/MessageGrouping.hpp"
#include "interactive/GadgetClassification.hpp"
#include "runtime/OrderedStringSet.hpp"
#include <functional>
#include <optional>
#include <unordered_map>

namespace batbox::interactive {

struct ReadSearchImports {
  gadgets::Gadgets gadgets;
  GadgetClassificationImports classification;
  std::function<std::optional<std::string>(const std::string&)>
      extractBashCommentLabel;
  std::function<std::string(const std::string&)> getDisplayPath;
  std::function<bool(const std::string&, const Json&)> isTeamMemoryWriteOrEdit;
  std::function<bool(const Json&)> isTeamMemorySearch;
  std::function<bool(const std::string&)> isTeamMemFile;
  std::function<Json(const std::string&, const std::string&)> detectGitOperation;
};

RenderItemsRef collapseReadSearchGroups(const RenderItemsRef& messages,
                                         const ReadSearchImports& imports);
std::string commandAsHint(const std::string& command);

} // namespace batbox::interactive
