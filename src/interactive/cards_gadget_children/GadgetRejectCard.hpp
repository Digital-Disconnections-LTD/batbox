#pragma once

#include "gadgets/Gadget.hpp"
#include <functional>
#include <optional>
#include <string>

namespace batbox::interactive::cards_gadget_children {

struct RejectRenderContext {
  int columns = 80;
  Json theme;
  const gadgets::Gadgets* gadgets = nullptr;
  bool verbose = false;
  bool transcriptMode = false;
  std::optional<std::string> style;
  Json filteredProgress = Json::array();
};

struct RejectServices {
  std::function<int()> readColumns;
  std::function<Json()> readTheme;
  std::function<std::optional<Json>(const gadgets::Gadget&,const Json&)>
      safeParseInput;
  std::function<Json(const gadgets::Gadget&,const Json&,
                     const RejectRenderContext&)> renderRejected;
};

struct GadgetRejectCard {
  bool fallback = true;
  Json renderedMessage = Json();
};

GadgetRejectCard classifyGadgetReject(const Json& input,
    const Json& progressMessages,const gadgets::Gadget* gadget,
    const gadgets::Gadgets& gadgets,std::optional<std::string> style,
    bool verbose,bool transcriptMode,const RejectServices& services);

} // namespace batbox::interactive::cards_gadget_children
