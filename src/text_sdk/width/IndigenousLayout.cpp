#include "text_sdk/width/IndigenousLayout.hpp"
#include "text_sdk/ansi_tokenize/AnsiTokenize.hpp"
#include "text_sdk/bun_text_width/Wrap.hpp"
#include "text_sdk/npm_wrap_ansi/Wrap.hpp"
#include <stdexcept>
namespace batbox::text_sdk::width {
WrapAnsiModule indigenousWrapAnsiModule(bool bunAvailable){
  return WrapAnsiModule({text_sdk::npm_wrap_ansi::indigenousWrapAnsiRef(),[bunAvailable]{
    return bunAvailable?text_sdk::bun_text_width::indigenousWrapAnsiRef():WrapAnsiRef{};
  }});
}
SliceServices indigenousSliceServices(std::shared_ptr<StringWidth> width){
  if(!width)throw std::invalid_argument("ANSI slice width provider required");
  SliceServices s;s.tokenize=text_sdk::ansi_tokenize::tokenize;
  s.reduceAnsiCodes=text_sdk::ansi_tokenize::reduceAnsiCodes;s.undoAnsiCodes=text_sdk::ansi_tokenize::undoAnsiCodes;
  s.ansiCodesToString=text_sdk::ansi_tokenize::ansiCodesToString;
  s.stringWidth=[width=std::move(width)](const std::optional<std::string>& text){
    return width->stringWidth(text?Json(*text):Json(Json::value_t::discarded));
  };
  return s;
}
}
