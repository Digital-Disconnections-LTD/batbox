#pragma once

#include "interactive/cards_gadget_result/HookProgressCard.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include <optional>

namespace batbox::interactive::cards_gadget_result {

std::optional<ftxui::Element> projectHookProgress(
    const HookProgressCard& card, ftxui::Color subtle);

} // namespace batbox::interactive::cards_gadget_result
