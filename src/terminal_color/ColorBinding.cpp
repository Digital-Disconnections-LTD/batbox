#include "terminal_color/Colorize.hpp"
#include "text_width/Layout.hpp"
#include "text_width/Tabs.hpp"
#include <stdexcept>

namespace batbox::terminal_color {
terminal_render::StyledTextImports bindStyledTextColor(
    const ChalkState& chalk,text_width::WrapServices wrapServices){
  return terminal_render::makeIndigenousStyledTextImports(
    [chalk](const std::string& text,
      const terminal_render::StylesRef& styles){
      return applyTextStyles(text,styles,chalk);
    },std::move(wrapServices));
}
terminal_render::RenderNodeImports bindRenderNodeImports(
    const ChalkState& chalk,text_width::WrapServices wrapServices,
    const std::shared_ptr<text_width::LineWidthStash>& lineWidthStash,
    std::function<std::optional<std::string>()> termProgram,
    std::function<bool()> isXtermJs){
  if(!lineWidthStash||!wrapServices.stringWidth||!termProgram||!isXtermJs)
    throw std::invalid_argument("Render-node native imports required");
  terminal_render::RenderNodeImports imports;
  imports.border=bindBorderColor(chalk,wrapServices.stringWidth);
  imports.text=bindStyledTextColor(chalk,std::move(wrapServices));
  imports.widestLine=[lineWidthStash](const std::string& value){
    return text_width::widestLine(value,
      [lineWidthStash](const std::string& line){
        return lineWidthStash->lineWidth(line);
      });
  };
  imports.termProgram=std::move(termProgram);
  imports.isXtermJs=std::move(isXtermJs);
  return imports;
}
} // namespace batbox::terminal_color
