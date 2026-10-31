#include "terminal_dom/DomIndigenousText.hpp"
#include "bun_text_width/Width.hpp"
#include "text_width/IndigenousLayout.hpp"
#include "text_width/Tabs.hpp"
#include <stdexcept>

namespace batbox::terminal_dom {
DomServicesRef indigenousDomServices(LayoutFactory layoutFactory,
    bool bunAvailable){
  if(!layoutFactory)throw std::invalid_argument("DOM layout factory required");
  auto intl=std::make_shared<intl::Stash>(intl::indigenousServices());
  auto widthServices=text_width::indigenousServices(intl);
  if(bunAvailable)widthServices.resolveBunStringWidth=[](){
    return bun_text_width::indigenousWidthCallback();};
  auto width=std::make_shared<text_width::StringWidth>(std::move(widthServices));
  auto lines=text_width::indigenousLineWidthStash(width);
  auto wrapModule=text_width::indigenousWrapAnsiModule(bunAvailable);
  auto wrapServices=text_width::indigenousLayoutServices(width,wrapModule.wrapAnsi());
  auto result=std::make_shared<DomServices>();
  result->createLayoutNode=std::move(layoutFactory);
  result->expandTabs=[width](const std::string& text){
    return text_width::expandTabs(text,[width](const std::string& part){
      return width->stringWidth(part);});};
  result->measureText=[lines](const std::string& text,double maxWidth){
    const auto measure=text_width::measureText(text,maxWidth,
      [lines](const std::string& line){return lines->lineWidth(line);});
    return terminal_screen::Size{measure.width,measure.height};};
  result->wrapText=[wrapServices=std::move(wrapServices)](
      const std::string& text,double maxWidth,const std::string& wrapType){
    return text_width::wrapText(text,maxWidth,wrapType,wrapServices);};
  return result;
}
} // namespace batbox::terminal_dom
