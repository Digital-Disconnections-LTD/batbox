#include "terminal_render/StyledText.hpp"
#include "terminal_dom/FullLayoutPort.hpp"
#include <cmath>
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
WrappedText wrapWithSoftWrap(const std::string& plainText,double maxWidth,
    const std::string& textWrap,const StyledTextImports& imports){
  if(!imports.wrapText)throw std::invalid_argument("Text wrap import required");
  if(textWrap!="wrap"&&textWrap!="wrap-trim")
    return {imports.wrapText(plainText,maxWidth,textWrap),{}};
  const auto origLines=splitLines(plainText);
  std::vector<std::string> outLines;
  auto softWrap=std::make_shared<std::vector<bool>>();
  for(const auto& original:origLines){
    const auto pieces=splitLines(imports.wrapText(original,maxWidth,textWrap));
    for(std::size_t i=0;i<pieces.size();i++){
      outLines.push_back(pieces[i]);softWrap->push_back(i>0);
    }
  }
  std::string joined;
  for(std::size_t i=0;i<outLines.size();i++){
    if(i)joined+='\n';
    joined+=outLines[i];
  }
  return {joined,softWrap};
}
std::string applyPaddingToText(const terminal_dom::DOMElement& node,
    std::string text,const std::shared_ptr<std::vector<bool>>& softWrap,
    const StyledTextImports& imports){
  const auto yoga=node.childNodes.empty()||!node.childNodes[0]?
    terminal_dom::LayoutRef{}:node.childNodes[0]->yogaNode;
  if(yoga){
    const auto full=std::dynamic_pointer_cast<terminal_dom::FullLayoutNode>(yoga);
    if(!full)throw std::invalid_argument("Full layout node import required");
    const auto offsetX=full->getComputedLeft();
    const auto offsetY=full->getComputedTop();
    const auto repeated=std::isnan(offsetY)?0:std::trunc(offsetY);
    if(!std::isfinite(repeated)||repeated<0)
      throw std::range_error("Invalid count value");
    if(!imports.indentString)
      throw std::invalid_argument("Indent-string import required");
    text=std::string(static_cast<std::size_t>(repeated),'\n')+
      imports.indentString(text,offsetX);
    if(softWrap&&offsetY>0){
      if(!std::isfinite(offsetY)||std::trunc(offsetY)!=offsetY||
          offsetY>4294967295.0)
        throw std::range_error("Invalid array length");
      softWrap->insert(softWrap->begin(),
        static_cast<std::size_t>(offsetY),false);
    }
  }
  return text;
}
} // namespace batbox::terminal_render
