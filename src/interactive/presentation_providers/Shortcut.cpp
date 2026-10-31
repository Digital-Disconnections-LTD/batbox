#include "interactive/presentation_providers/Shortcut.hpp"
#include "runtime/Encoding.hpp"
#include "runtime/JsonValue.hpp"
#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace batbox::interactive::presentation_providers {
namespace {
std::string lower(std::string text){
  for(auto& ch:text)ch=static_cast<char>(std::tolower(
    static_cast<unsigned char>(ch)));
  return text;
}
std::string displayName(const std::string& key){
  if(key=="escape")return "Esc";
  if(key==" ")return "Space";
  if(key=="enter")return "Enter";
  if(key=="backspace")return "Backspace";
  if(key=="delete")return "Delete";
  if(key=="up")return "↑";
  if(key=="down")return "↓";
  if(key=="left")return "←";
  if(key=="right")return "→";
  if(key=="pageup")return "PageUp";
  if(key=="pagedown")return "PageDown";
  if(key=="home")return "Home";
  if(key=="end")return "End";
  return key;
}
std::string join(const std::vector<std::string>& parts,const char* separator){
  std::string result;
  for(const auto& part:parts){
    if(!result.empty())result+=separator;
    result+=part;
  }
  return result;
}
} // namespace

Keystroke parseKeystroke(const std::string& input){
  Keystroke result;
  std::size_t start=0;
  do{
    const auto end=input.find('+',start);
    const auto part=lower(input.substr(start,end==std::string::npos?
      std::string::npos:end-start));
    if(part=="ctrl"||part=="control")result.ctrl=true;
    else if(part=="alt"||part=="opt"||part=="option")result.alt=true;
    else if(part=="shift")result.shift=true;
    else if(part=="meta")result.meta=true;
    else if(part=="cmd"||part=="command"||part=="super"||part=="win")
      result.super=true;
    else if(part=="esc")result.key="escape";
    else if(part=="return")result.key="enter";
    else if(part=="space")result.key=" ";
    else if(part=="↑")result.key="up";
    else if(part=="↓")result.key="down";
    else if(part=="←")result.key="left";
    else if(part=="→")result.key="right";
    else result.key=part;
    if(end==std::string::npos)break;
    start=end+1;
  }while(true);
  return result;
}
Chord parseChord(const std::string& input){
  if(input==" ")return {parseKeystroke("space")};
  const auto trimmed=value::trim(input);
  Chord result;
  std::size_t start=0;
  while(start<trimmed.size()){
    auto end=start;
    while(end<trimmed.size()&&
          !std::isspace(static_cast<unsigned char>(trimmed[end])))++end;
    result.push_back(parseKeystroke(trimmed.substr(start,end-start)));
    start=end;
    while(start<trimmed.size()&&
          std::isspace(static_cast<unsigned char>(trimmed[start])))++start;
  }
  if(result.empty())result.push_back(parseKeystroke(""));
  return result;
}
std::string keystrokeToString(const Keystroke& ks){
  std::vector<std::string> parts;
  if(ks.ctrl)parts.push_back("ctrl");
  if(ks.alt)parts.push_back("alt");
  if(ks.shift)parts.push_back("shift");
  if(ks.meta)parts.push_back("meta");
  if(ks.super)parts.push_back("cmd");
  parts.push_back(displayName(ks.key));
  return join(parts,"+");
}
std::string chordToString(const Chord& chord){
  std::vector<std::string> parts;
  for(const auto& stroke:chord)parts.push_back(keystrokeToString(stroke));
  return join(parts," ");
}
std::string keystrokeToDisplayString(const Keystroke& ks,
    const std::string& platform){
  std::vector<std::string> parts;
  if(ks.ctrl)parts.push_back("ctrl");
  if(ks.alt||ks.meta)parts.push_back(platform=="macos"?"opt":"alt");
  if(ks.shift)parts.push_back("shift");
  if(ks.super)parts.push_back(platform=="macos"?"cmd":"super");
  parts.push_back(displayName(ks.key));
  return join(parts,"+");
}
std::string chordToDisplayString(const Chord& chord,
    const std::string& platform){
  std::vector<std::string> parts;
  for(const auto& stroke:chord)
    parts.push_back(keystrokeToDisplayString(stroke,platform));
  return join(parts," ");
}
std::vector<Binding> parseBindings(const std::vector<BindingBlock>& blocks){
  std::vector<Binding> result;
  for(const auto& block:blocks)
    for(const auto& [key,action]:block.bindings)
      result.push_back({parseChord(key),action,block.context});
  return result;
}
std::optional<std::string> getBindingDisplayText(const std::string& action,
    const std::string& context,const std::vector<Binding>& bindings){
  for(auto it=bindings.rbegin();it!=bindings.rend();++it)
    if(it->action&&*it->action==action&&it->context==context)
      return chordToString(it->chord);
  return {};
}
std::string ShortcutProvider::display(const std::string& action,
    const std::string& context,const std::string& fallback)const{
  if(!loadBindings)throw std::logic_error("Keybinding loader required");
  return getBindingDisplayText(action,context,loadBindings()).value_or(fallback);
}
std::string shortcutHintText(const std::string& shortcut,
    const std::string& action,bool parens){
  const auto value=shortcut+" to "+action;
  return parens?"("+value+")":value;
}
ftxui::Element shortcutHint(const std::string& shortcut,
    const std::string& action,bool parens,bool bold){
  const auto prefix=parens?"(":"";
  const auto suffix=parens?")":"";
  auto key=ftxui::text(value::encodeUtf8(shortcut));
  if(bold)key=key|ftxui::bold;
  return ftxui::hbox({ftxui::text(prefix),std::move(key),
    ftxui::text(value::encodeUtf8(" to "+action+suffix))});
}
std::string configurableShortcutHintText(const ShortcutProvider& provider,
    const std::string& action,const std::string& context,
    const std::string& fallback,const std::string& description,
    bool parens){
  return shortcutHintText(provider.display(action,context,fallback),
    description,parens);
}
ftxui::Element configurableShortcutHint(const ShortcutProvider& provider,
    const std::string& action,const std::string& context,
    const std::string& fallback,const std::string& description,
    bool parens,bool bold){
  return shortcutHint(provider.display(action,context,fallback),
    description,parens,bold);
}
} // namespace batbox::interactive::presentation_providers
