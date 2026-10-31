#pragma once
#include "runtime/JsonValue.hpp"
#include <ftxui/dom/node.hpp>
#include <ftxui/screen/color.hpp>

namespace batbox::interactive::presentation_providers {
Json backgroundFooterMissions(const Json& missions);
std::size_t visibleFooterMissions(const Json& missions,int terminalRows);
ftxui::Element missionFooter(const Json& missions,int columns,int terminalRows,
  ftxui::Color color);
}
