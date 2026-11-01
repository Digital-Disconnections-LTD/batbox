#include "interactive/markdown/IndigenousRenderer.hpp"
#include "npm_wrap_ansi/Wrap.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>
#include <utility>

namespace batbox::interactive::markdown {

std::shared_ptr<Renderer> makeIndigenousRenderer(
    std::shared_ptr<text_width::StringWidth> width,
    int terminalColumns,bool colors,bool hyperlinks,
    std::string permissionSgr,
    std::optional<SyntaxHighlighter> highlighter,
    std::optional<SemanticAccents> accents,
    FormatCustomizer customize){
  if(!width)throw std::invalid_argument("Markdown Unicode width provider required");
  auto measure=[width](const std::string& text){
    const auto value=width->stringWidth(Json(text));
    if(!std::isfinite(value)||value<0||
       value>std::numeric_limits<int>::max()||std::trunc(value)!=value)
      throw std::runtime_error("Invalid Markdown terminal width");
    return static_cast<int>(value);
  };
  auto services=indigenousFormatServices(colors,hyperlinks,
    std::move(permissionSgr),std::move(measure));
  if(colors&&accents){
    services.listMarker=[sgr=accents->listMarkerSgr](const std::string& text){
      return sgr.empty()?text:sgr+text+"\x1b[39m";};
    services.strongLeadIn=[sgr=accents->strongLeadInSgr](const std::string& text){
      return sgr.empty()?text:sgr+text+"\x1b[39m";};
  }
  if(highlighter){
    if(!highlighter->supportsLanguage||!highlighter->highlight||
       !highlighter->debug)
      throw std::invalid_argument("Incomplete Markdown highlighter provider");
    services.supportsLanguage=std::move(highlighter->supportsLanguage);
    services.highlight=std::move(highlighter->highlight);
    services.debug=std::move(highlighter->debug);
  }
  if(customize)customize(services);
  WrapAnsi wrap=[](const std::string& text,int columns,bool hard){
    return npm_wrap_ansi::wrapAnsi(text,columns,
      Json{{"hard",hard},{"trim",false},{"wordWrap",true}});
  };
  return std::make_shared<Renderer>(std::move(services),std::move(wrap),
    terminalColumns);
}
} // namespace batbox::interactive::markdown
