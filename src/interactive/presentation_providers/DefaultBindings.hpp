#pragma once
#include "interactive/presentation_providers/Shortcut.hpp"
#include <functional>
#include <string>
namespace batbox::interactive::presentation_providers {
struct DefaultBindingOptions {
  std::string platform;
  bool terminalVtMode=true;
  std::function<bool(const std::string&)> feature;
};
std::vector<BindingBlock> defaultBindingBlocks(const DefaultBindingOptions&);
std::vector<Binding> defaultBindings(const DefaultBindingOptions&);
ShortcutProvider makeShortcutProvider(DefaultBindingOptions,
    std::function<std::vector<Binding>()> loadUserBindings);
} // namespace batbox::interactive::presentation_providers
