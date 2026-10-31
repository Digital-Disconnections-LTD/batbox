#include "interactive/cards_assistant/AssistantProjection.hpp"
#include "runtime/Encoding.hpp"
#include <stdexcept>
#include <utility>
#include <vector>

namespace batbox::interactive::cards_assistant {
namespace {
ftxui::Element withMargin(ftxui::Element content, bool margin) {
  return margin ? ftxui::vbox({ftxui::text(""), std::move(content)}) : content;
}

ftxui::Element wrappedLines(const std::string& text, ftxui::Color color) {
  std::vector<ftxui::Element> lines;
  std::size_t start = 0;
  do {
    const auto end = text.find('\n', start);
    lines.push_back(ftxui::paragraph(value::encodeUtf8(
        text.substr(start, end == std::string::npos ? end : end - start))) |
        ftxui::color(color));
    if (end == std::string::npos) break;
    start = end + 1;
  } while (true);
  return ftxui::vbox(std::move(lines));
}

ftxui::Element response(ftxui::Element child, const Palette& palette,
                        bool oneRow) {
  auto line = ftxui::hbox({
      ftxui::text("  ⎿  ") | ftxui::color(palette.subtle),
      std::move(child) | ftxui::xflex
  });
  if (oneRow) line = line | ftxui::size(ftxui::HEIGHT, ftxui::EQUAL, 1);
  return line;
}

ftxui::Element requiredShortcut(const ProjectionImports& imports) {
  if (!imports.renderExpandHint)
    throw std::logic_error("Expand shortcut renderer required");
  return imports.renderExpandHint();
}
} // namespace

std::optional<ftxui::Element> projectAssistantText(
    const Card& card, const ProjectionImports& imports) {
  using K = Kind;
  if (card.kind == K::Hidden) return {};
  if (card.kind == K::RateLimit) {
    if (!imports.renderRateLimit)
      throw std::logic_error("Rate limit renderer required");
    return imports.renderRateLimit(card.displayText);
  }
  if (card.kind == K::Markdown) {
    if (!imports.renderMarkdown)
      throw std::logic_error("Markdown renderer required");
    auto content = imports.renderMarkdown(card.displayText, false);
    if (card.shouldShowDot) {
      if (imports.dotGlyph.empty())
        throw std::logic_error("Assistant dot glyph required");
      content = ftxui::hbox({
        ftxui::text(value::encodeUtf8(imports.dotGlyph)) |
          ftxui::color(card.selected ? imports.palette.suggestion :
                       imports.palette.text) |
          ftxui::size(ftxui::WIDTH, ftxui::GREATER_THAN, 2),
        std::move(content) | ftxui::xflex
      });
    }
    if (card.selected)
      content = content | ftxui::bgcolor(imports.palette.selectedBackground);
    return withMargin(std::move(content), card.addMargin);
  }
  if (card.kind == K::Interrupted) {
    auto child = wrappedLines(card.displayText, imports.palette.subtle);
    return response(std::move(child), imports.palette, true);
  }
  if (card.kind == K::ModelUnavailable) {
    const auto split = card.displayText.find('\n');
    auto child = ftxui::vbox({
      wrappedLines(card.displayText.substr(0, split), imports.palette.error),
      ftxui::text(""),
      wrappedLines(split == std::string::npos ? "" :
        card.displayText.substr(split + 1), imports.palette.text)
    });
    return response(std::move(child), imports.palette, false);
  }
  auto child = wrappedLines(card.displayText, imports.palette.error);
  if (card.kind == K::ApiError && card.truncated)
    child = ftxui::vbox({std::move(child), requiredShortcut(imports)});
  const bool oneRow = card.kind == K::ContextLimit ||
      card.kind == K::CreditBalance || card.kind == K::ExternalInvalidKey ||
      card.kind == K::TokenRevoked || card.kind == K::Timeout;
  return response(std::move(child), imports.palette, oneRow);
}

std::optional<ftxui::Element> projectThinking(
    const ThinkingCard& card, const ProjectionImports& imports) {
  using K = ThinkingKind;
  if (card.kind == K::Hidden) return {};
  if (card.kind == K::Redacted)
    return withMargin(ftxui::text("✻ Thinking…") |
        ftxui::color(imports.palette.subtle) | ftxui::dim | ftxui::italic,
        card.addMargin);
  if (card.kind == K::Collapsed) {
    auto heading = ftxui::hbox({
      ftxui::text("∴ Thinking "), requiredShortcut(imports)
    }) | ftxui::color(imports.palette.subtle) | ftxui::dim |
      ftxui::italic;
    return withMargin(std::move(heading), card.addMargin);
  }
  if (!imports.renderMarkdown)
    throw std::logic_error("Markdown renderer required");
  auto child = ftxui::vbox({
    ftxui::text("∴ Thinking…") | ftxui::color(imports.palette.subtle) |
      ftxui::dim | ftxui::italic,
    ftxui::text(""),
    ftxui::hbox({ftxui::text("  "),
                imports.renderMarkdown(card.thinking, true) | ftxui::xflex})
  });
  return withMargin(std::move(child), card.addMargin);
}

ftxui::Element projectRateLimitCard(const RateLimitCard& card,
                                    const Palette& palette) {
  std::vector<ftxui::Element> children;
  children.push_back(wrappedLines(card.text, palette.error));
  if (card.upsell)
    children.push_back(wrappedLines(*card.upsell, palette.subtle));
  return response(ftxui::vbox(std::move(children)), palette, false);
}

} // namespace batbox::interactive::cards_assistant
