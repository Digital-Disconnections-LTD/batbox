#pragma once

#include <ftxui/screen/color.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace batbox::interactive::presentation_providers {

using RawTheme=std::unordered_map<std::string,std::string>;
RawTheme lightTheme();
RawTheme lightAnsiTheme();
RawTheme darkTheme();
RawTheme darkAnsiTheme();
RawTheme lightDaltonizedTheme();
RawTheme darkDaltonizedTheme();
RawTheme batboxDarkTheme();
RawTheme batboxLightTheme();
RawTheme batboxAnsiTheme();

const RawTheme& getTheme(std::string_view name);
std::vector<std::string> themeNames();
std::string resolveThemeName(std::string_view setting,std::string_view systemTheme);
std::optional<std::string> rawColor(std::string_view theme,
    std::string_view key);
std::optional<ftxui::Color> parseColor(std::string_view raw);
ftxui::Color themeColor(std::string_view theme,std::string_view key);

} // namespace batbox::interactive::presentation_providers
