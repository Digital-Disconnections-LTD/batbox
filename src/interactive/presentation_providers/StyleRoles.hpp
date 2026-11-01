#pragma once

#include <ftxui/screen/color.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace batbox::interactive::presentation_providers {

enum class ColorDepth {None, Ansi16, Ansi256, TrueColor};

// Semantic roles that every Batbox surface paints with. Body text and the
// screen background stay with the terminal; a role holding Color::Default
// inherits the terminal color. Surfaces read roles, never raw theme keys.
struct StyleRoles {
  std::string theme;
  bool light=false;
  // No color is available: bold, dim, underline and glyphs carry meaning.
  bool monochrome=false;
  // The terminal palette is used as is; panel tints are disabled.
  bool basic=false;

  ftxui::Color brand,brandDeep,lantern,mint,cream;
  ftxui::Color text,muted,faint;

  ftxui::Color userBar,userText,userPanel;
  ftxui::Color assistantMarker,thinking;

  ftxui::Color gadgetName,gadgetTarget,gadgetRunning,gadgetOk,gadgetErr;

  ftxui::Color codeBg,codeText,codeLabel,codeFrame,inlineCode,inlineCodeBg;
  ftxui::Color heading,headingGlyph,headingMinorGlyph,link,listMarker,quote;
  ftxui::Color syntaxKeyword,syntaxString,syntaxNumber,syntaxComment,syntaxType;

  ftxui::Color diffAdd,diffAddBg,diffDel,diffDelBg,diffContext,diffLineNo,diffHunk;

  ftxui::Color dimLabel,kbd,border,borderFocus,selection,selectionBg,spinner;
  ftxui::Color statusOk,statusWarn,statusErr;
  ftxui::Color modeDefault,modePlan,modeAccept,modeYolo;
};

// Roles are resolved once per theme name and cached. Call
// invalidateStyleRoles() after changing the FTXUI color depth so cached colors
// are rebuilt with the new quantization.
const StyleRoles& styleRoles(std::string_view theme);
void invalidateStyleRoles();

struct ThemeChoice {
  std::string name,label,description;
};
// Themes offered by /theme, in display order, starting with "auto".
std::vector<ThemeChoice> selectableThemes();
bool isKnownTheme(std::string_view name);
// Accepts legacy aliases ("halloween") and returns the canonical name.
std::string canonicalThemeName(std::string_view name);

struct TerminalEnvironment {
  std::string term,colorTerm,termProgram,noColor,wtSession;
  bool windows=false;
};
ColorDepth detectColorDepth(const TerminalEnvironment& environment);
// Records the depth Batbox paints for and aligns FTXUI's color quantization.
void applyColorDepth(ColorDepth depth);
ColorDepth activeColorDepth();
// The theme a terminal of this depth can show for a requested theme name.
std::string adaptThemeToDepth(std::string_view theme,ColorDepth depth);
// "auto" follows the detected terminal background ("light" or "dark").
std::string themeForSetting(std::string_view setting,ColorDepth depth,
    std::string_view systemTheme);

// SGR parameter sequence for a role ("\x1b[...m"), empty for Color::Default.
std::string sgr(const ftxui::Color& color,bool background=false);
bool isDefault(const ftxui::Color& color);

} // namespace batbox::interactive::presentation_providers
