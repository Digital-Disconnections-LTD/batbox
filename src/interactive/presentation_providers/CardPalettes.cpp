#include "interactive/presentation_providers/CardPalettes.hpp"

namespace batbox::interactive::presentation_providers {
cards::OutputLinePalette outputLinePalette(const ThemeState& state){
  const auto theme=state.currentTheme();
  return {themeColor(theme,"subtle"),themeColor(theme,"text"),
    themeColor(theme,"error"),themeColor(theme,"warning")};
}
cards::UserCardPalette userCardPalette(const ThemeState& state){
  const auto theme=state.currentTheme();
  return {themeColor(theme,"bashBorder"),themeColor(theme,"text"),
    themeColor(theme,"subtle"),themeColor(theme,"bashMessageBackgroundColor"),
    themeColor(theme,"userMessageBackground")};
}
cards::PoetryPalette poetryPalette(const ThemeState& state){
  const auto theme=state.currentTheme();
  return {themeColor(theme,"userMessageBackground"),
    themeColor(theme,"selectionBg"),[theme](const auto& value){
      return rawColor(theme,value)?themeColor(theme,value):
        parseColor(value).value_or(ftxui::Color::Default);
    }};
}
cards_assistant::Palette assistantPalette(const ThemeState& state){
  const auto theme=state.currentTheme();
  return {themeColor(theme,"text"),themeColor(theme,"subtle"),
    themeColor(theme,"error"),themeColor(theme,"suggestion"),
    themeColor(theme,"selectionBg")};
}
cards_system::SystemPalette systemPalette(const ThemeState& state){
  const auto theme=state.currentTheme();
  return {themeColor(theme,"text"),themeColor(theme,"subtle"),
    themeColor(theme,"error"),themeColor(theme,"selectionBg")};
}
cards_gadget_result::GadgetErrorPalette gadgetErrorPalette(const ThemeState& state){
  const auto theme=state.currentTheme();
  return {outputLinePalette(state),themeColor(theme,"subtle")};
}
cards_remaining::AttachmentPalette attachmentPalette(const ThemeState& state){
  const auto theme=state.currentTheme();
  return {themeColor(theme,"text"),themeColor(theme,"subtle"),
    themeColor(theme,"error"),themeColor(theme,"warning"),
    themeColor(theme,"success"),"●"};
}
cards_remaining::LeafPalette leafPalette(const ThemeState& state,
    std::function<bool(bool)> isBlinking){
  const auto theme=state.currentTheme();
  return {themeColor(theme,"text"),themeColor(theme,"subtle"),
    themeColor(theme,"success"),themeColor(theme,"error"),
    "●","✓",std::move(isBlinking)};
}
} // namespace batbox::interactive::presentation_providers
