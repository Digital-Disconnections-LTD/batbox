#include "text_width/IndigenousLayout.hpp"
#include "ansi_tokenize/AnsiTokenize.hpp"
#include "bun_text_width/Wrap.hpp"
#include "npm_wrap_ansi/Wrap.hpp"
#include <stdexcept>
namespace batbox::text_width {
WrapAnsiModule indigenousWrapAnsiModule(bool bunAvailable){
  return WrapAnsiModule({npm_wrap_ansi::indigenousWrapAnsiRef(),[bunAvailable]{
    return bunAvailable?bun_text_width::indigenousWrapAnsiRef():WrapAnsiRef{};
  }});
}
SliceServices indigenousSliceServices(std::shared_ptr<StringWidth> width){
  if(!width)throw std::invalid_argument("ANSI slice width provider required");
  SliceServices s;s.tokenize=ansi_tokenize::tokenize;
  s.reduceAnsiCodes=ansi_tokenize::reduceAnsiCodes;s.undoAnsiCodes=ansi_tokenize::undoAnsiCodes;
  s.ansiCodesToString=ansi_tokenize::ansiCodesToString;
  s.stringWidth=[width=std::move(width)](const std::optional<std::string>& text){
    return width->stringWidth(text?Json(*text):Json(Json::value_t::discarded));
  };
  return s;
}
WrapServices indigenousLayoutServices(std::shared_ptr<StringWidth> width,WrapAnsiRef wrap){
  if(!width||!wrap||!*wrap)throw std::invalid_argument("Native layout width and wrapping imports required");
  WrapServices services;bindAnsiSlice(services,indigenousSliceServices(width));
  services.stringWidth=[width=std::move(width)](const std::string& text){return width->stringWidth(text);};
  services.wrapAnsi=[wrap=std::move(wrap)](const std::string& text,double columns,bool trim,bool hard){
    return (*wrap)(text,columns,Json{{"trim",trim},{"hard",hard}});
  };
  return services;
}
}
