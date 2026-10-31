#include "interactive/ReadSearchInternal.hpp"
#include <stdexcept>

namespace batbox::interactive::read_search_internal {
void scanBashResultForGitOps(const RenderItem& item, Group& group,
                              const ReadSearchImports& imports) {
  if (!item.message || value(*item.message, "type") != "user") return;
  const auto* output = member(*item.message, "toolUseResult");
  if (!output) return;
  const auto stdoutText = value(*output, "stdout");
  const auto stderrText = value(*output, "stderr");
  if (stdoutText.empty() && stderrText.empty()) return;
  const auto combined = stdoutText + "\n" + stderrText;
  const auto* inner = member(*item.message, "message");
  const auto* content = inner ? member(*inner, "content") : nullptr;
  if (!content || !content->is_array()) return;
  for (const auto& block : *content) {
    if (value(block, "type") != "tool_result") continue;
    const auto found = group.bashCommands.find(value(block, "tool_use_id"));
    if (found == group.bashCommands.end() || found->second.empty()) continue;
    if (!imports.detectGitOperation)
      throw std::logic_error("detectGitOperation import required");
    const auto operations = imports.detectGitOperation(found->second, combined);
    bool any = false;
    for (const auto& [key, into] : {
         std::pair<const char*, Json*>{"commit", &group.commits},
         {"push", &group.pushes}, {"branch", &group.branches},
         {"pr", &group.prs}}) {
      const auto* operation = member(operations, key);
      if (operation && value::truthy(*operation)) {
        into->push_back(*operation);
        any = true;
      }
    }
    if (any) group.gitOpBashCount = group.gitOpBashCount.value_or(0) + 1;
  }
}
} // namespace batbox::interactive::read_search_internal
