#pragma once

#include "interactive/gadget_views/GadgetViewModel.hpp"
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <string>

namespace batbox::interactive::gadget_views {

struct GadgetViewStyle {
  ftxui::Color text=ftxui::Color::Default,muted=ftxui::Color::Default,
    faint=ftxui::Color::Default,ok=ftxui::Color::Default,
    warn=ftxui::Color::Default,error=ftxui::Color::Default;
  ftxui::Color diffAdd=ftxui::Color::Default,diffAddBg=ftxui::Color::Default,
    diffDel=ftxui::Color::Default,diffDelBg=ftxui::Color::Default,
    diffContext=ftxui::Color::Default,diffLineNo=ftxui::Color::Default,
    diffHunk=ftxui::Color::Default;
  bool monochrome=false;
  std::string expandKey="ctrl+o";
};

// Result rows hang from a "⎿" connector two columns in, so every Gadget's
// output lines up under its header name.
inline constexpr int resultIndent=5;
ftxui::Element gadgetResponse(ftxui::Element body,const GadgetViewStyle& style);

// The body of a view, without the connector. Columns is the width available
// to the body.
ftxui::Element projectGadgetView(const Json& view,const GadgetViewStyle& style,
    int columns);
// Any value a Gadget renderer returns, framed under the connector: views by
// kind, strings as sanitized text, other Json by its visible text member.
ftxui::Element projectGadgetResult(const Json& value,const GadgetViewStyle& style,
    int columns);
// A finished background job as one card row, like a Gadget header:
// "✓ Bat reviewer finished · 3 Gadgets · 12s · Found two parser bugs".
ftxui::Element projectMissionNotice(const MissionNotice& notice,
    const GadgetViewStyle& style);
// Header text after the Gadget name: one row, ellipsis when narrow.
ftxui::Element projectGadgetInline(const Json& value,ftxui::Color color);

} // namespace batbox::interactive::gadget_views
