#pragma once

#include "interactive/cards_gadget_result/GadgetResultRouter.hpp"
#include "interactive/cards_gadget_result/GadgetErrorCard.hpp"
#include "interactive/cards_gadget_result/GadgetErrorProjection.hpp"
#include "ftxui/dom/elements.hpp"
#include <functional>
#include <optional>

namespace batbox::interactive::cards_gadget_result {

struct GadgetResultProjectionImports {
  GadgetResultProtocol resultProtocol;
  GadgetErrorProtocol errorProtocol;
  GadgetErrorPalette palette;
  int columns = 80;
  bool classifierFeature = false;
  std::function<std::string()> shortcutDisplay;
  std::function<std::optional<ftxui::Element>(
      const GadgetResultCard&, const Json&)> renderRejected;
  std::function<std::optional<ftxui::Element>(
      const std::string&)> renderRejectedPlan;
  std::function<std::optional<ftxui::Element>(
      const GadgetResultCard&, const Json&, const Json&)> renderGadgetError;
  std::function<std::optional<ftxui::Element>(
      const GadgetResultCard&, const Json&)> renderSuccess;
};

std::optional<ftxui::Element> projectGadgetResultBlock(
    const Json& param, const Json& progressMessages,
    const MessageLookups& lookups, const gadgets::Gadgets& gadgets,
    bool verbose, const GadgetResultProjectionImports& imports);

} // namespace batbox::interactive::cards_gadget_result
