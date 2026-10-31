#include "interactive/cards/UserBashOutputProjection.hpp"
#include "interactive/cards/UserTextLeaves.hpp"

namespace batbox::interactive::cards {

ftxui::Element projectUserBashOutput(const std::string& sourceText,
                                      bool verbose,
                                      const BashResultProjectionImports& imports) {
  const auto leaf = bashOutputLeaf(sourceText, verbose);
  const Json content{{"stdout", leaf.stdoutText}, {"stderr", leaf.stderrText}};
  return projectBashResult(buildBashResultCard(content, leaf.verbose), imports);
}
} // namespace batbox::interactive::cards
