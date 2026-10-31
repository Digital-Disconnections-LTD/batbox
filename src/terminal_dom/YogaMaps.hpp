#pragma once
#include "indigenous_yoga/Enums.hpp"
#include <string_view>

namespace batbox::terminal_dom::yoga_map {
indigenous_yoga::Edge edge(std::string_view name);
indigenous_yoga::Gutter gutter(std::string_view name);
indigenous_yoga::FlexDirection flexDirection(std::string_view name);
indigenous_yoga::Align align(std::string_view name);
indigenous_yoga::Justify justify(std::string_view name);
indigenous_yoga::Wrap wrap(std::string_view name);
indigenous_yoga::Overflow overflow(std::string_view name);
indigenous_yoga::PositionType positionType(std::string_view name);
} // namespace batbox::terminal_dom::yoga_map
