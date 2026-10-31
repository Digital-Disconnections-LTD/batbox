#pragma once

#include "interactive/cards_gadget_children/GadgetRejectCard.hpp"
#include "ftxui/dom/elements.hpp"
#include "ftxui/screen/color.hpp"
#include <functional>

namespace batbox::interactive::cards_gadget_children {

ftxui::Element projectGadgetReject(const GadgetRejectCard& card,
    ftxui::Color subtle,
    const std::function<ftxui::Element(const Json&)>& renderGadgetMessage);

} // namespace batbox::interactive::cards_gadget_children
