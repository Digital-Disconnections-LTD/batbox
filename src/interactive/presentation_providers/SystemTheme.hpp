#pragma once

#include <functional>
#include <optional>
#include <string>

namespace batbox::interactive::presentation_providers {

std::optional<std::string> themeFromOscColor(const std::string& data);
std::optional<std::string> detectFromColorFgBg(const std::string& value);

class SystemThemeStash {
public:
  explicit SystemThemeStash(std::function<std::optional<std::string>()> colorFgBg);
  std::string current();
  void set(std::string theme);
private:
  std::function<std::optional<std::string>()> colorFgBg_;
  std::optional<std::string> stashed_;
};

} // namespace batbox::interactive::presentation_providers
