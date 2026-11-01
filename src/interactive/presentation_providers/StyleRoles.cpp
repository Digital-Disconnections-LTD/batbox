#include "interactive/presentation_providers/StyleRoles.hpp"
#include "interactive/presentation_providers/ThemeData.hpp"
#include <ftxui/screen/terminal.hpp>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <map>
#include <memory>
#include <mutex>

namespace batbox::interactive::presentation_providers {
namespace {
std::string lower(std::string_view value){
  std::string result(value);
  std::transform(result.begin(),result.end(),result.begin(),[](unsigned char ch){
    return static_cast<char>(std::tolower(ch));});
  return result;
}
bool contains(const std::string& haystack,std::initializer_list<const char*> needles){
  for(const auto* needle:needles)
    if(haystack.find(needle)!=std::string::npos)return true;
  return false;
}
std::atomic<ColorDepth>& depthSlot(){
  static std::atomic<ColorDepth> depth{ColorDepth::TrueColor};
  return depth;
}
struct RoleCache {
  std::mutex mutex;
  std::map<std::string,std::unique_ptr<StyleRoles>,std::less<>> entries;
};
RoleCache& cache(){static RoleCache value;return value;}

ftxui::Color pick(const RawTheme& theme,const char* role,
    std::initializer_list<const char*> fallbacks){
  const auto parse=[&](const std::string& key)->std::optional<ftxui::Color>{
    const auto found=theme.find(key);
    if(found==theme.end())return {};
    if(found->second=="default")return ftxui::Color::Default;
    return parseColor(found->second);
  };
  if(auto value=parse(std::string("role.")+role))return *value;
  for(const auto* key:fallbacks)if(auto value=parse(key))return *value;
  return ftxui::Color::Default;
}

StyleRoles resolve(const std::string& name){
  const auto& theme=getTheme(name);
  StyleRoles roles;
  roles.theme=name;
  roles.monochrome=name=="monochrome";
  roles.basic=roles.monochrome||name.ends_with("ansi");
  roles.light=name.starts_with("light")||name=="batbox-light";
  if(roles.monochrome)return roles;
  // Batbox themes name every role; older palettes derive them from the keys
  // their cards already used.
  roles.brand=pick(theme,"brand",{"brand"});
  roles.brandDeep=pick(theme,"brandDeep",{"permission"});
  roles.lantern=pick(theme,"lantern",{"warning"});
  roles.mint=pick(theme,"mint",{"success"});
  roles.cream=pick(theme,"cream",{"text"});
  roles.text=pick(theme,"text",{});
  roles.muted=pick(theme,"muted",{"inactive"});
  roles.faint=pick(theme,"faint",{"subtle"});
  roles.userBar=pick(theme,"userBar",{"briefLabelYou"});
  roles.userText=pick(theme,"userText",{});
  roles.userPanel=pick(theme,"userPanel",{"userMessageBackground"});
  roles.assistantMarker=pick(theme,"assistantMarker",{"brand"});
  roles.thinking=pick(theme,"thinking",{"inactive"});
  roles.gadgetName=pick(theme,"gadgetName",{});
  roles.gadgetTarget=pick(theme,"gadgetTarget",{});
  roles.gadgetRunning=pick(theme,"gadgetRunning",{"brandBlueSpinner"});
  roles.gadgetOk=pick(theme,"gadgetOk",{"success"});
  roles.gadgetErr=pick(theme,"gadgetErr",{"error"});
  roles.codeBg=pick(theme,"codeBg",{"bashMessageBackgroundColor"});
  roles.codeText=pick(theme,"codeText",{});
  roles.codeLabel=pick(theme,"codeLabel",{"inactive"});
  roles.codeFrame=pick(theme,"codeFrame",{"promptBorder"});
  roles.inlineCode=pick(theme,"inlineCode",{"permission"});
  roles.inlineCodeBg=pick(theme,"inlineCodeBg",{});
  roles.heading=pick(theme,"heading",{});
  roles.headingGlyph=pick(theme,"headingGlyph",{"brand"});
  roles.headingMinorGlyph=pick(theme,"headingMinorGlyph",{"permission"});
  roles.link=pick(theme,"link",{"ide"});
  roles.listMarker=pick(theme,"listMarker",{"brand"});
  roles.quote=pick(theme,"quote",{"inactive"});
  roles.syntaxKeyword=pick(theme,"syntaxKeyword",{"autoAccept"});
  roles.syntaxString=pick(theme,"syntaxString",{"success"});
  roles.syntaxNumber=pick(theme,"syntaxNumber",{"warning"});
  roles.syntaxComment=pick(theme,"syntaxComment",{"inactive"});
  roles.syntaxType=pick(theme,"syntaxType",{"permission"});
  roles.diffAdd=pick(theme,"diffAdd",{"diffAddedWord","success"});
  roles.diffAddBg=pick(theme,"diffAddBg",{"diffAdded"});
  roles.diffDel=pick(theme,"diffDel",{"diffRemovedWord","error"});
  roles.diffDelBg=pick(theme,"diffDelBg",{"diffRemoved"});
  roles.diffContext=pick(theme,"diffContext",{"inactive"});
  roles.diffLineNo=pick(theme,"diffLineNo",{"subtle"});
  roles.diffHunk=pick(theme,"diffHunk",{"inactive"});
  roles.dimLabel=pick(theme,"dimLabel",{"inactive"});
  roles.kbd=pick(theme,"kbd",{});
  roles.border=pick(theme,"border",{"promptBorder"});
  roles.borderFocus=pick(theme,"borderFocus",{"permission"});
  roles.selection=pick(theme,"selection",{"suggestion"});
  roles.selectionBg=pick(theme,"selectionBg",{"selectionBg"});
  roles.spinner=pick(theme,"spinner",{"brandBlueSpinner"});
  roles.statusOk=pick(theme,"statusOk",{"success"});
  roles.statusWarn=pick(theme,"statusWarn",{"warning"});
  roles.statusErr=pick(theme,"statusErr",{"error"});
  roles.modeDefault=pick(theme,"modeDefault",{"inactive"});
  roles.modePlan=pick(theme,"modePlan",{"planMode"});
  roles.modeAccept=pick(theme,"modeAccept",{"autoAccept"});
  roles.modeYolo=pick(theme,"modeYolo",{"error"});
  if(roles.basic){
    // A 16-color palette cannot express subtle panel tints.
    roles.userPanel=roles.codeBg=roles.inlineCodeBg=roles.diffAddBg=
      roles.diffDelBg=roles.selectionBg=ftxui::Color::Default;
  }
  return roles;
}
} // namespace

const StyleRoles& styleRoles(std::string_view theme){
  auto& roles=cache();
  std::lock_guard lock(roles.mutex);
  if(const auto found=roles.entries.find(theme);found!=roles.entries.end())
    return *found->second;
  auto created=std::make_unique<StyleRoles>(resolve(std::string(theme)));
  auto& stored=*created;
  roles.entries.emplace(std::string(theme),std::move(created));
  return stored;
}
void invalidateStyleRoles(){
  auto& roles=cache();
  std::lock_guard lock(roles.mutex);
  // Callers hold references only for one paint; retire entries by moving
  // them aside so a reference taken earlier in this paint stays valid.
  static std::vector<std::unique_ptr<StyleRoles>> retired;
  for(auto& [name,entry]:roles.entries)retired.push_back(std::move(entry));
  roles.entries.clear();
}

std::vector<ThemeChoice> selectableThemes(){
  return {
    {"auto","Match terminal","Batbox dark or light, following the terminal background"},
    {"batbox-dark","Batbox dark","Night purple, lantern orange and mint for dark terminals"},
    {"batbox-light","Batbox light","Plum, ember and teal ink for light terminals"},
    {"batbox-ansi","16-color","Uses the terminal's own palette; no background tints"},
    {"monochrome","Monochrome","No color: bold, dim and underline only"},
    {"dark-daltonized","Color-blind dark","Blue and orange contrast for dark terminals"},
    {"light-daltonized","Color-blind light","Blue and orange contrast for light terminals"},
    {"dark","Classic dark","The original dark palette"},
    {"light","Classic light","The original light palette"},
  };
}
std::string canonicalThemeName(std::string_view name){
  const auto value=lower(name);
  if(value=="halloween")return "batbox-dark";
  if(value=="halloween-ansi")return "batbox-ansi";
  return value;
}
bool isKnownTheme(std::string_view name){
  const auto value=canonicalThemeName(name);
  if(value=="auto")return true;
  for(const auto& known:themeNames())if(known==value)return true;
  return false;
}

ColorDepth detectColorDepth(const TerminalEnvironment& env){
  const auto term=lower(env.term),colorTerm=lower(env.colorTerm),
    program=lower(env.termProgram);
  if(!env.noColor.empty()||term=="dumb")return ColorDepth::None;
  if(contains(colorTerm,{"truecolor","24bit"}))return ColorDepth::TrueColor;
  // Terminals that advertise themselves through TERM_PROGRAM render 24-bit
  // color even when TERM still says xterm-256color.
  if(contains(program,{"iterm","wezterm","ghostty","vscode","warp","hyper","tabby","rio"}))
    return ColorDepth::TrueColor;
  if(!env.wtSession.empty())return ColorDepth::TrueColor;
  if(contains(term,{"direct","truecolor","kitty","alacritty","foot","ghostty","wezterm"}))
    return ColorDepth::TrueColor;
  if(program=="apple_terminal")return ColorDepth::Ansi256;
  if(env.windows)return ColorDepth::TrueColor;
  if(contains(colorTerm,{"256"})||contains(term,{"256","xterm","screen","tmux"}))
    return ColorDepth::Ansi256;
  return ColorDepth::Ansi16;
}
void applyColorDepth(ColorDepth depth){
  depthSlot().store(depth);
  ftxui::Terminal::SetColorSupport(
    depth==ColorDepth::None?ftxui::Terminal::Color::Palette1:
    depth==ColorDepth::Ansi16?ftxui::Terminal::Color::Palette16:
    depth==ColorDepth::Ansi256?ftxui::Terminal::Color::Palette256:
    ftxui::Terminal::Color::TrueColor);
  invalidateStyleRoles();
}
ColorDepth activeColorDepth(){return depthSlot().load();}
std::string adaptThemeToDepth(std::string_view requested,ColorDepth depth){
  const auto theme=canonicalThemeName(requested);
  if(depth==ColorDepth::None)return "monochrome";
  if(depth!=ColorDepth::Ansi16)return theme;
  if(theme=="batbox-dark"||theme=="batbox-light")return "batbox-ansi";
  if(theme=="dark"||theme=="dark-daltonized")return "dark-ansi";
  if(theme=="light"||theme=="light-daltonized")return "light-ansi";
  return theme;
}
std::string themeForSetting(std::string_view setting,ColorDepth depth,
    std::string_view systemTheme){
  const auto chosen=canonicalThemeName(setting);
  if(chosen=="auto"||chosen.empty())
    return adaptThemeToDepth(systemTheme=="light"?"batbox-light":"batbox-dark",depth);
  return adaptThemeToDepth(chosen,depth);
}
bool isDefault(const ftxui::Color& color){return color==ftxui::Color::Default;}
std::string sgr(const ftxui::Color& color,bool background){
  if(isDefault(color))return {};
  return "\x1b["+color.Print(background)+"m";
}
} // namespace batbox::interactive::presentation_providers
