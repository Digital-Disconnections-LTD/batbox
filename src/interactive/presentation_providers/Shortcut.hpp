#pragma once

#include <ftxui/dom/elements.hpp>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace batbox::interactive::presentation_providers {

struct Keystroke {
  std::string key;
  bool ctrl=false,alt=false,shift=false,meta=false,super=false;
};
using Chord=std::vector<Keystroke>;
struct Binding {
  Chord chord;
  std::optional<std::string> action;
  std::string context;
};
struct BindingBlock {
  std::string context;
  std::vector<std::pair<std::string,std::optional<std::string>>> bindings;
};

Keystroke parseKeystroke(const std::string& input);
Chord parseChord(const std::string& input);
std::string keystrokeToString(const Keystroke& keystroke);
std::string chordToString(const Chord& chord);
std::string keystrokeToDisplayString(const Keystroke& keystroke,
    const std::string& platform="linux");
std::string chordToDisplayString(const Chord& chord,
    const std::string& platform="linux");
std::vector<Binding> parseBindings(const std::vector<BindingBlock>& blocks);
std::optional<std::string> getBindingDisplayText(const std::string& action,
    const std::string& context,const std::vector<Binding>& bindings);

struct ShortcutDisplayCache {
  std::mutex mutex;
  std::map<std::string,std::string> text;
};
struct ShortcutProvider {
  std::function<std::vector<Binding>()> loadBindings;
  // Key hints are drawn every frame; each action's text is resolved from the
  // bindings once per provider. Build a new provider to change bindings.
  std::shared_ptr<ShortcutDisplayCache> displayCache=
    std::make_shared<ShortcutDisplayCache>();
  std::string display(const std::string& action,const std::string& context,
    const std::string& fallback)const;
};
std::string shortcutHintText(const std::string& shortcut,
    const std::string& action,bool parens=false);
ftxui::Element shortcutHint(const std::string& shortcut,
    const std::string& action,bool parens=false,bool bold=false);
std::string configurableShortcutHintText(const ShortcutProvider& provider,
    const std::string& action,const std::string& context,
    const std::string& fallback,const std::string& description,
    bool parens=false);
ftxui::Element configurableShortcutHint(const ShortcutProvider& provider,
    const std::string& action,const std::string& context,
    const std::string& fallback,const std::string& description,
    bool parens=false,bool bold=false);

} // namespace batbox::interactive::presentation_providers
