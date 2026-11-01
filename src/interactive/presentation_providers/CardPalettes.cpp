#include "interactive/presentation_providers/CardPalettes.hpp"
#include "interactive/presentation_providers/StyleRoles.hpp"

namespace batbox::interactive::presentation_providers {
cards::OutputLinePalette outputLinePalette(const ThemeState& state){
  const auto& roles=styleRoles(state.currentTheme());
  return {roles.muted,roles.text,roles.statusErr,roles.statusWarn};
}
cards::UserCardPalette userCardPalette(const ThemeState& state){
  const auto& roles=styleRoles(state.currentTheme());
  return {roles.lantern,roles.userText,roles.muted,roles.codeBg,roles.userPanel};
}
cards::PoetryPalette poetryPalette(const ThemeState& state){
  const auto theme=state.currentTheme();
  const auto& roles=styleRoles(theme);
  // The pointer is the only "subtle" run in a regular user turn; it shares the
  // input prompt's role so a sent message looks like what was typed.
  return {roles.userPanel,roles.selectionBg,[theme,&roles](const auto& value){
      if(value=="subtle")return roles.userBar;
      if(value=="text")return roles.userText;
      if(value=="suggestion")return roles.selection;
      return rawColor(theme,value)?themeColor(theme,value):
        parseColor(value).value_or(ftxui::Color::Default);
    }};
}
cards_assistant::Palette assistantPalette(const ThemeState& state){
  const auto& roles=styleRoles(state.currentTheme());
  return {roles.text,roles.muted,roles.statusErr,roles.selection,
    roles.selectionBg};
}
cards_system::SystemPalette systemPalette(const ThemeState& state){
  const auto& roles=styleRoles(state.currentTheme());
  return {roles.text,roles.muted,roles.statusErr,roles.selectionBg};
}
cards_gadget_result::GadgetErrorPalette gadgetErrorPalette(const ThemeState& state){
  const auto& roles=styleRoles(state.currentTheme());
  return {outputLinePalette(state),roles.muted};
}
cards_remaining::AttachmentPalette attachmentPalette(const ThemeState& state){
  const auto& roles=styleRoles(state.currentTheme());
  return {roles.text,roles.muted,roles.statusErr,roles.statusWarn,
    roles.statusOk,"●"};
}
cards_remaining::LeafPalette leafPalette(const ThemeState& state,
    std::function<bool(bool)> isBlinking){
  const auto& roles=styleRoles(state.currentTheme());
  return {roles.text,roles.muted,roles.statusOk,roles.statusErr,
    "●","✓",std::move(isBlinking)};
}
gadget_views::GadgetViewStyle gadgetViewStyle(const StyleRoles& roles,
    std::string expandKey){
  gadget_views::GadgetViewStyle style;
  style.text=roles.text;style.muted=roles.muted;style.faint=roles.faint;
  style.ok=roles.statusOk;style.warn=roles.statusWarn;style.error=roles.statusErr;
  style.diffAdd=roles.diffAdd;style.diffAddBg=roles.diffAddBg;
  style.diffDel=roles.diffDel;style.diffDelBg=roles.diffDelBg;
  style.diffContext=roles.diffContext;style.diffLineNo=roles.diffLineNo;
  style.diffHunk=roles.diffHunk;style.monochrome=roles.monochrome;
  style.expandKey=std::move(expandKey);
  return style;
}
terminal_ftxui::InlinePalette inlinePalette(const ThemeState& state){
  const auto& roles=styleRoles(state.currentTheme());
  terminal_ftxui::InlinePalette palette;
  palette.user=roles.userBar;palette.assistant=roles.text;
  palette.reasoning=roles.thinking;palette.gadget=roles.brand;
  palette.gadgetResult=roles.muted;palette.progress=roles.muted;
  palette.system=roles.mint;palette.error=roles.statusErr;
  return palette;
}
} // namespace batbox::interactive::presentation_providers
