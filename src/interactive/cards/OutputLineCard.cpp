#include "interactive/cards/OutputLineCard.hpp"

namespace batbox::interactive::cards {
OutputLineCard outputLineCard(const std::string& content,
                              const OutputLineOptions& options,
                              const OutputLineImports& imports) {
  auto formatted = tryJsonFormatContent(content);
  if (options.linkifyUrls) formatted = linkifyUrlsInText(formatted, imports);
  const bool full = options.verbose || options.expandShellOutput;
  formatted = stripUnderlineAnsi(full ? formatted :
      renderTruncatedContent(formatted, options.columns,
                             options.inVirtualList, imports));
  return {std::move(formatted), options.isError ?
      std::optional<std::string>{"error"} : options.isWarning ?
      std::optional<std::string>{"warning"} : std::nullopt};
}
} // namespace batbox::interactive::cards
