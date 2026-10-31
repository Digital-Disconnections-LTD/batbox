#include "terminal_render/StyledText.hpp"
#include "terminal_render/IndigenousIndent.hpp"
#include "text_width/Layout.hpp"
#include <stdexcept>

namespace batbox::terminal_render {
StyledTextImports makeIndigenousStyledTextImports(
    std::function<std::string(const std::string&,const StylesRef&)> applyTextStyles,
    text_width::WrapServices wrapServices){
  if(!applyTextStyles||!wrapServices.stringWidth||
      !wrapServices.sliceAnsi||!wrapServices.wrapAnsi)
    throw std::invalid_argument("Native style and text-wrap imports required");
  StyledTextImports result;
  result.applyTextStyles=std::move(applyTextStyles);
  result.wrapText=[services=std::move(wrapServices)](
      const std::string& value,double width,const std::string& type){
    return text_width::wrapText(value,width,type,services);};
  result.indentString=indigenousIndentString;
  return result;
}
} // namespace batbox::terminal_render
