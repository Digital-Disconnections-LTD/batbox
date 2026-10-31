#include "terminal_render/RenderNodeInternal.hpp"
#include "terminal_screen/TypedArraySemantics.hpp"
#include <stdexcept>

namespace batbox::terminal_render {
namespace {
std::vector<std::string> splitLines(const std::string& text){
  std::vector<std::string> result;
  std::size_t start=0;
  while(true){
    const auto end=text.find('\n',start);
    if(end==std::string::npos){result.push_back(text.substr(start));break;}
    result.push_back(text.substr(start,end-start));start=end+1;
  }
  return result;
}
}
void RenderNodeRenderer::renderText(terminal_dom::DOMElement& node,
    terminal_output::Output& output,
    const terminal_dom::FullLayoutNode& yoga,double x,double y,
    const std::optional<std::string>& inheritedBackgroundColor){
  auto inherited=std::make_shared<Json>(Json::object());
  if(inheritedBackgroundColor&&!inheritedBackgroundColor->empty())
    (*inherited)["backgroundColor"]=*inheritedBackgroundColor;
  const auto segments=squashTextNodesToSegments(node,inherited);
  std::string plainText;
  for(const auto& segment:*segments)plainText+=segment.text;
  if(plainText.empty())return;
  const auto maxWidth=terminal_screen::typed_array::jsMin(
    getMaxWidth(yoga),output.width-x);
  const auto textWrap=detail::string(node,"textWrap").value_or("wrap");
  if(!imports_.widestLine)
    throw std::invalid_argument("Widest-line import required");
  const bool needsWrapping=imports_.widestLine(plainText)>maxWidth;
  std::string text;
  std::shared_ptr<std::vector<bool>> softWrap;
  if(needsWrapping&&segments->size()==1){
    const auto& segment=segments->at(0);
    const auto wrapped=wrapWithSoftWrap(plainText,maxWidth,textWrap,
      imports_.text);
    softWrap=wrapped.softWrap;
    const auto lines=splitLines(wrapped.wrapped);
    if(!imports_.text.applyTextStyles)
      throw std::invalid_argument("Text style import required");
    for(std::size_t i=0;i<lines.size();i++){
      if(i)text+='\n';
      auto styled=imports_.text.applyTextStyles(lines[i],segment.styles);
      if(segment.hyperlink&&!segment.hyperlink->empty())
        styled=wrapWithOsc8Link(styled,*segment.hyperlink);
      text+=styled;
    }
  }else if(needsWrapping){
    const auto wrapped=wrapWithSoftWrap(plainText,maxWidth,textWrap,
      imports_.text);
    softWrap=wrapped.softWrap;
    const auto charToSegment=buildCharToSegmentMap(*segments);
    text=applyStylesToWrappedText(wrapped.wrapped,*segments,charToSegment,
      plainText,textWrap=="wrap-trim",imports_.text);
  }else{
    if(!imports_.text.applyTextStyles)
      throw std::invalid_argument("Text style import required");
    for(const auto& segment:*segments){
      auto styled=imports_.text.applyTextStyles(segment.text,segment.styles);
      if(segment.hyperlink&&!segment.hyperlink->empty())
        styled=wrapWithOsc8Link(styled,*segment.hyperlink);
      text+=styled;
    }
  }
  text=applyPaddingToText(node,std::move(text),softWrap,imports_.text);
  output.write(x,y,std::move(text),softWrap);
}
} // namespace batbox::terminal_render
